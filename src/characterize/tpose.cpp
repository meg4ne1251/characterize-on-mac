#include "scene.h"
#include <algorithm>
#include <cmath>

namespace cmac {
static constexpr double pi=3.14159265358979323846;
struct Segment { const char* from; const char* to; FbxVector4 target; };
static std::vector<Segment> segments() {
    return {{"LeftArm","LeftForeArm",FbxVector4(1,0,0)},
            {"LeftForeArm","LeftHand",FbxVector4(1,0,0)},
            {"RightArm","RightForeArm",FbxVector4(-1,0,0)},
            {"RightForeArm","RightHand",FbxVector4(-1,0,0)},
            {"LeftUpLeg","LeftLeg",FbxVector4(0,-1,0)},
            {"LeftLeg","LeftFoot",FbxVector4(0,-1,0)},
            {"RightUpLeg","RightLeg",FbxVector4(0,-1,0)},
            {"RightLeg","RightFoot",FbxVector4(0,-1,0)}};
}
static FbxVector4 difference(FbxNode* a,FbxNode* b) {
    auto v=global(b).GetT()-global(a).GetT(); v[3]=0; return v;
}
static double angle(FbxVector4 a,FbxVector4 b) {
    double la=a.Length(),lb=b.Length();
    if(la<1e-9||lb<1e-9) return 180.0;
    return std::acos(std::clamp(a.DotProduct(b)/(la*lb),-1.0,1.0))*180.0/pi;
}
Json poseAngles(const Bones& bones) {
    Json result=Json::array();
    for(auto& seg:segments()) if(bones.count(seg.from)&&bones.count(seg.to)) {
        Json x=Json::object(); x["from"]=seg.from; x["to"]=seg.to;
        auto direction=difference(bones.at(seg.from),bones.at(seg.to));
        x["degrees"]=angle(direction,seg.target); x["length"]=direction.Length(); result.push(x);
    }
    return result;
}
void checkPose(const Bones& bones,Report& report,double tolerance) {
    for(auto& seg:segments()) if(bones.count(seg.from)&&bones.count(seg.to)) {
        auto direction=difference(bones.at(seg.from),bones.at(seg.to));
        if(direction.Length()<1e-9) report.add("error","pose.zero_length","長さがゼロのボーンです: "+std::string(seg.from));
        else if(angle(direction,seg.target)>tolerance) {
            Json d=Json::object(); d["from"]=seg.from; d["to"]=seg.to; d["degrees"]=angle(direction,seg.target);
            report.add("error","pose.not_tpose","Tポーズから外れています: "+std::string(seg.from),d);
        }
    }
    for(auto side:{"Left","Right"}) {
        auto foot=std::string(side)+"Foot",toe=std::string(side)+"ToeBase";
        if(bones.count(foot)&&bones.count(toe)) {
            auto dir=difference(bones.at(foot),bones.at(toe));
            if(dir[2]<=0) report.add("warning","pose.foot_direction","つま先が +Z を向いていません: "+toe);
        }
    }
}
static bool uniform(const FbxVector4& scale) {
    // Blender FBX exports in the supplied models contain float noise of a few ppm.
    const double tolerance=1e-4*std::max({1.0,std::abs(scale[0]),std::abs(scale[1]),std::abs(scale[2])});
    return scale[0]>1e-8 && scale[1]>1e-8 && scale[2]>1e-8 &&
           std::abs(scale[0]-scale[1])<tolerance && std::abs(scale[1]-scale[2])<tolerance;
}
void correctPose(Scene& s,const Bones& bones,Report& report,double maxAngle) {
    validateSkeleton(bones,report);
    for(const auto& sl:slots()) if(sl.required&&!bones.count(sl.name)) report.add("error","pose.missing_bone","Tポーズ補正の必須ボーンがありません: "+sl.name);
    for(int i=0;i<s.scene->GetSrcObjectCount<FbxAnimCurve>();++i)
        if(s.scene->GetSrcObject<FbxAnimCurve>(i)->KeyGetCount()>0) {report.add("error","pose.animation","アニメーション付き FBX の自動補正は未対応です。静止モデルを使用してください。");break;}
    for(int i=0;i<s.scene->GetSrcObjectCount<FbxConstraint>();++i)
        if(!FbxCast<FbxCharacter>(s.scene->GetSrcObject<FbxConstraint>(i))) {report.add("error","pose.constraint","拘束を含むシーンの自動補正は未対応です。");break;}
    for(auto n:s.nodes()) {
        if(n->GetMesh()&&n->GetMesh()->GetDeformerCount(FbxDeformer::eVertexCache)) report.add("error","pose.vertex_cache","頂点キャッシュを含むモデルの補正は未対応です。");
    }
    for(auto [slot,n]:bones) for(auto p=n;p && p!=s.scene->GetRootNode();p=p->GetParent()) {
        if(!uniform(FbxVector4(p->LclScaling.Get()))) {report.add("error","pose.scale","負または非一様スケールがあります: "+nodePath(p));break;}
        if(!uniform(global(p).GetS())) {report.add("error","pose.global_scale","グローバル変換に負または非一様スケールがあります: "+nodePath(p));break;}
    }
    if(!report.ok()) return;
    if(bones.count("LeftUpLeg")&&bones.count("RightUpLeg")) {
        auto across=difference(bones.at("RightUpLeg"),bones.at("LeftUpLeg"));
        if(angle(across,FbxVector4(1,0,0))>20) report.add("error","pose.facing","左右の腰の位置が +Z 前向きの規約と一致しません。モデルの向きを確認してください。");
    }
    if(angle(difference(bones.at("Hips"),bones.at("Head")),FbxVector4(0,1,0))>20)
        report.add("error","pose.torso","胴体の傾きが大きいため自動補正できません。");
    for(auto& seg:segments()) {
        auto dir=difference(bones.at(seg.from),bones.at(seg.to));
        if(dir.Length()<1e-9) report.add("error","pose.zero_length","長さゼロのボーンです: "+std::string(seg.from));
        // Test the entire limb using its initial orientation; rotations later are measured again.
        if(angle(dir,seg.target)>maxAngle) report.add("error","pose.max_angle","補正角度の上限を超えます: "+std::string(seg.from));
    }
    if(!report.ok()) return;
    Json before=poseAngles(bones), rotations=Json::array();
    std::map<FbxNode*,FbxDouble3> oldRotations,oldTranslations,oldScales;
    for(auto& seg:segments()) {
        auto n=bones.at(seg.from),child=bones.at(seg.to);
        oldRotations[n]=n->LclRotation.Get(); oldTranslations[n]=n->LclTranslation.Get(); oldScales[n]=n->LclScaling.Get();
        auto dir=difference(n,child); dir.Normalize(); auto target=seg.target; target.Normalize();
        double degrees=angle(dir,target);
        if(degrees<1e-7) continue;
        if(degrees>maxAngle) {
            for(auto [changed,r]:oldRotations){changed->LclRotation.Set(r);changed->LclTranslation.Set(oldTranslations.at(changed));changed->LclScaling.Set(oldScales.at(changed));}
            s.scene->GetAnimationEvaluator()->Reset();
            report.add("error","pose.max_angle","親の補正後に必要な回転が上限を超えました。変更を取り消しました: "+std::string(seg.from));return;
        }
        auto cross=dir.CrossProduct(target);
        FbxQuaternion delta(cross[0],cross[1],cross[2],1+dir.DotProduct(target)); delta.Normalize();
        auto original=global(n); FbxAMatrix desired;
        desired.SetTQS(original.GetT(),delta.Product(original.GetQ()),original.GetS());
        FbxVector4 t,r,scale;
        s.scene->GetAnimationEvaluator()->ComputeLocalTRSFromGlobal(t,r,scale,n,desired,FBXSDK_TIME_INFINITE,FbxNode::eSourcePivot,false,true);
        n->LclTranslation.Set(FbxDouble3(t[0],t[1],t[2]));
        n->LclRotation.Set(FbxDouble3(r[0],r[1],r[2]));
        n->LclScaling.Set(FbxDouble3(scale[0],scale[1],scale[2]));
        s.scene->GetAnimationEvaluator()->Reset();
        Json op=Json::object(); op["bone"]=n->GetName(); op["rotation_degrees"]=degrees; rotations.push(op);
    }
    bool aligned=true;
    for(auto& seg:segments()) if(angle(difference(bones.at(seg.from),bones.at(seg.to)),seg.target)>0.01) aligned=false;
    if(!aligned) {
        for(auto [n,r]:oldRotations) {n->LclRotation.Set(r);n->LclTranslation.Set(oldTranslations.at(n));n->LclScaling.Set(oldScales.at(n));}
        s.scene->GetAnimationEvaluator()->Reset();
        report.add("error","pose.unsupported_transform","ピボット・継承変換を含む補正が収束しませんでした。変更を取り消しました。");return;
    }
    Json op=Json::object(); op["tpose_before"]=before; op["tpose_after"]=poseAngles(bones); op["rotations"]=rotations; report.operations.push(op);
    report.add("info","pose.corrected","腕を水平・脚を垂直に補正しました。頂点・シェイプ・ウェイト・バインド行列は保持しています。指、肩の巻き込み、接地、見た目は確認してください。");
}
}
