#include "scene.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <chrono>
#include "audit.h"

using namespace cmac;
static int assertions=0;
void require(bool ok,const std::string& message) {++assertions;if(!ok)throw std::runtime_error(message);}
bool near(const FbxVector4& a,const FbxVector4& b,double tolerance=1e-6){return (a-b).Length()<tolerance;}
FbxNode* bone(Scene& s,Bones& bones,const std::string& slot,const std::string& parent,FbxDouble3 t) {
    std::string name;
    for(auto& sl:slots())if(sl.name==slot)name=sl.bone;
    auto n=FbxNode::Create(s.scene,name.c_str());auto sk=FbxSkeleton::Create(s.scene,(name+"_attr").c_str());
    sk->SetSkeletonType(parent.empty()?FbxSkeleton::eRoot:FbxSkeleton::eLimbNode);n->SetNodeAttribute(sk);
    n->LclTranslation.Set(t);(parent.empty()?s.scene->GetRootNode():bones.at(parent))->AddChild(n);bones[slot]=n;return n;
}
Bones fixture(Scene& s,const fs::path& folder,bool apose=true) {
    s.scene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::Motionbuilder);
    Bones b;
    bone(s,b,"Reference","",FbxDouble3(0,0,0));bone(s,b,"Hips","Reference",FbxDouble3(0,100,0));
    bone(s,b,"Spine","Hips",FbxDouble3(0,10,0));bone(s,b,"Spine1","Spine",FbxDouble3(0,10,0));
    bone(s,b,"Spine2","Spine1",FbxDouble3(0,10,0));bone(s,b,"Neck","Spine2",FbxDouble3(0,10,0));bone(s,b,"Head","Neck",FbxDouble3(0,10,0));
    for(auto side:{"Left","Right"}) {
        std::string p=side;double sign=p=="Left"?1:-1;
        bone(s,b,p+"Shoulder","Spine2",FbxDouble3(sign*5,5,0));
        auto arm=bone(s,b,p+"Arm",p+"Shoulder",FbxDouble3(sign*10,0,0));
        bone(s,b,p+"ForeArm",p+"Arm",FbxDouble3(sign*20,0,0));bone(s,b,p+"Hand",p+"ForeArm",FbxDouble3(sign*15,0,0));
        if(apose)arm->LclRotation.Set(FbxDouble3(0,0,-sign*35));
        auto leg=bone(s,b,p+"UpLeg","Hips",FbxDouble3(sign*5,-5,0));
        bone(s,b,p+"Leg",p+"UpLeg",FbxDouble3(0,-40,0));bone(s,b,p+"Foot",p+"Leg",FbxDouble3(0,-40,0));
        bone(s,b,p+"ToeBase",p+"Foot",FbxDouble3(0,-2,10));if(apose)leg->LclRotation.Set(FbxDouble3(0,0,sign*3));
        for(auto finger:{"Thumb","Index","Middle","Ring","Pinky"}) {
            std::string f=finger;
            bone(s,b,p+"InHand"+f,p+"Hand",FbxDouble3(sign*2,0,1));
            for(int i=1;i<=4;++i)bone(s,b,p+"Hand"+f+std::to_string(i),i==1?p+"InHand"+f:p+"Hand"+f+std::to_string(i-1),FbxDouble3(sign*2,0,0));
        }
    }
    auto node=FbxNode::Create(s.scene,"Face");auto mesh=FbxMesh::Create(s.scene,"FaceMesh");node->SetNodeAttribute(mesh);s.scene->GetRootNode()->AddChild(node);
    mesh->InitControlPoints(3);auto points=mesh->GetControlPoints();
    points[0]=global(b.at("LeftForeArm")).GetT();points[1]=global(b.at("LeftHand")).GetT();points[2]=points[0]+FbxVector4(0,0,1,0);
    mesh->BeginPolygon();mesh->AddPolygon(0);mesh->AddPolygon(1);mesh->AddPolygon(2);mesh->EndPolygon();
    auto uv=mesh->CreateElementUV("UVMap");uv->SetMappingMode(FbxLayerElement::eByControlPoint);uv->SetReferenceMode(FbxLayerElement::eDirect);
    uv->GetDirectArray().Add(FbxVector2(0,0));uv->GetDirectArray().Add(FbxVector2(1,0));uv->GetDirectArray().Add(FbxVector2(0,1));
    auto skin=FbxSkin::Create(s.scene,"Skin");mesh->AddDeformer(skin);
    for(auto slot:{"LeftArm","LeftForeArm"}) {
        auto cluster=FbxCluster::Create(s.scene,slot);cluster->SetLink(b.at(slot));cluster->SetLinkMode(FbxCluster::eNormalize);
        cluster->SetTransformMatrix(global(node));cluster->SetTransformLinkMatrix(global(b.at(slot)));
        if(std::string(slot)=="LeftArm"){cluster->AddControlPointIndex(0,1);cluster->AddControlPointIndex(2,1);}else cluster->AddControlPointIndex(1,1);
        skin->AddCluster(cluster);
    }
    auto blend=FbxBlendShape::Create(s.scene,"Expressions");mesh->AddDeformer(blend);
    for(auto vowel:{"a","i","u","o"}) {
        auto channel=FbxBlendShapeChannel::Create(s.scene,vowel);auto shape=FbxShape::Create(s.scene,vowel);shape->InitControlPoints(3);
        for(int i=0;i<3;++i)shape->GetControlPoints()[i]=points[i]+FbxVector4(0,0,0.2,0);
        channel->AddTargetShape(shape);blend->AddBlendShapeChannel(channel);
    }
    auto mat=FbxSurfacePhong::Create(s.scene,"Body");node->AddMaterial(mat);
    auto matLayer=mesh->CreateElementMaterial();matLayer->SetMappingMode(FbxLayerElement::eAllSame);matLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);matLayer->GetIndexArray().Add(0);
    fs::create_directories(folder);s.source=folder/"fixture.fbx";
    // Valid 1x1 PNG, so the fixture remains independently viewable.
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
    std::ofstream image(folder/"Body.png",std::ios::binary);image.write(reinterpret_cast<const char*>(png),sizeof(png));
    return b;
}
FbxNode* meshNode(Scene& s){for(auto n:s.nodes())if(n->GetMesh())return n;throw std::runtime_error("mesh missing");}
FbxVector4 deformed(Scene& s,int cp) {
    auto node=meshNode(s);auto mesh=node->GetMesh();auto skin=static_cast<FbxSkin*>(mesh->GetDeformer(0,FbxDeformer::eSkin));
    FbxVector4 result(0,0,0,0);double total=0;
    for(int c=0;c<skin->GetClusterCount();++c){
        auto cluster=skin->GetCluster(c);FbxAMatrix bindMesh,bindBone;cluster->GetTransformMatrix(bindMesh);cluster->GetTransformLinkMatrix(bindBone);
        auto matrix=global(cluster->GetLink())*bindBone.Inverse()*bindMesh;
        for(int j=0;j<cluster->GetControlPointIndicesCount();++j)if(cluster->GetControlPointIndices()[j]==cp){double w=cluster->GetControlPointWeights()[j];result+=matrix.MultT(mesh->GetControlPoints()[cp])*w;total+=w;}
    }
    return total?result/total:mesh->GetControlPoints()[cp];
}
void pipeline(Scene& s,Bones& bones,const fs::path& folder,Report& report) {
    correctPose(s,bones,report,75);applyTextures(s,folder,report);createCharacter(s,bones,"Test_Miku",true,report);createGroups(s,bones,"Test_Miku",report);validate(s,bones,report,0.01);
}
#include "shaders.h"
#include "media.h"
int main(int argc,char** argv){
    try {
        if(argc==4&&std::string(argv[1])=="--audit") {
            bool passed=false;auto result=auditFiles(argv[2],argv[3],&passed);std::cout<<result.dump()<<'\n';return passed?0:2;
        }
        auto root=fs::absolute(argc>1?argv[1]:"build/test-data");fs::create_directories(root);
        testShaders(root);
        testEmbeddedMedia(root);
        testTextureMapping(root);
        Scene s;auto b=fixture(s,root);require(b.size()==73,"all 73 slots in fixture");
        auto before=deformed(s,0);require(near(before,global(b.at("LeftForeArm")).GetT()),"skin initially matches arm endpoint");
        auto points=meshNode(s)->GetMesh()->GetControlPoints();FbxVector4 original=points[0];
        auto skin=static_cast<FbxSkin*>(meshNode(s)->GetMesh()->GetDeformer(0,FbxDeformer::eSkin));FbxAMatrix originalBind;skin->GetCluster(0)->GetTransformLinkMatrix(originalBind);
        s.save(root/"apose.fbx");
        Report report;pipeline(s,b,root,report);require(report.ok(),"pipeline findings: "+report.json().dump());
        require(near(deformed(s,0),global(b.at("LeftForeArm")).GetT()),"skinned vertex follows corrected arm");
        require(!near(before,deformed(s,0)),"skin actually moved from A pose");
        require(near(original,meshNode(s)->GetMesh()->GetControlPoints()[0]),"base mesh unchanged");
        FbxAMatrix bindAfter;skin->GetCluster(0)->GetTransformLinkMatrix(bindAfter);for(int row=0;row<4;++row)require(near(originalBind.GetRow(row),bindAfter.GetRow(row)),"bind matrices unchanged");
        for(auto& sl:slots()){
            char* sdkName=nullptr;require(FbxCharacter::GetCharacterNodeNameFromNodeId(sl.id,sdkName)&&sdkName&&sl.name==sdkName,"SDK slot identifier: "+sl.name);
            FbxCharacterLink link;require(s.scene->GetCharacter(0)->GetCharacterLink(sl.id,&link)&&link.mNode==b.at(sl.name),"slot roundtrip: "+sl.name);
        }
        auto position=deformed(s,0);s.save(root/"configured.fbx");
        Scene reopened;reopened.load(root/"configured.fbx");Report rr;auto rb=resolve(reopened,rr,"",true);validate(reopened,rb,rr,0.01);
        require(rr.ok(),"reopen checks: "+rr.json().dump());require(near(position,deformed(reopened,0)),"deformed vertex survives export");
        require(reopened.scene->GetCharacter(0)->Characterize.Get(),"characterize flag exported");
        Report idempotent;correctPose(reopened,rb,idempotent,75);require(idempotent.ok(),"idempotent correction");require(near(position,deformed(reopened,0)),"second correction unchanged");
        {
            Scene scale;auto sb=fixture(scale,root);sb.at("LeftArm")->LclScaling.Set(FbxDouble3(1,2,1));Report r;correctPose(scale,sb,r,75);require(!r.ok(),"nonuniform scaling rejected");
        }
        {
            Scene animated;auto ab=fixture(animated,root);auto stack=FbxAnimStack::Create(animated.scene,"Take");auto layer=FbxAnimLayer::Create(animated.scene,"Base");stack->AddMember(layer);
            auto curve=ab.at("LeftArm")->LclRotation.GetCurve(layer,FBXSDK_CURVENODE_COMPONENT_Z,true);curve->KeyModifyBegin();auto i=curve->KeyAdd(FBXSDK_TIME_ZERO);curve->KeySetValue(i,10);curve->KeyModifyEnd();
            Report r;correctPose(animated,ab,r,75);require(!r.ok(),"keyframed scene rejected");animated.save(root/"animated.fbx");
        }
        {
            Scene collision;auto cb=fixture(collision,root,false);auto n=FbxNode::Create(collision.scene,"Spine1");collision.scene->GetRootNode()->AddChild(n);Report r;renameBones(collision,cb,r);require(!r.ok(),"rename collision rejected");require(std::string(cb.at("Spine")->GetName())=="Spine1","collision makes no changes");
        }
        {
            Scene pivots;auto pb=fixture(pivots,root);pb.at("LeftArm")->SetPreRotation(FbxNode::eSourcePivot,FbxVector4(8,12,6));pb.at("LeftArm")->SetPostRotation(FbxNode::eSourcePivot,FbxVector4(2,1,3));
            Report r;correctPose(pivots,pb,r,75);require(r.ok(),"pre/post rotations supported: "+r.json().dump());checkPose(pb,r,0.01);require(r.ok(),"pre/post rotations align");
        }
        {
            Scene duplicate;auto db=fixture(duplicate,root,false);Report r;pipeline(duplicate,db,root,r);require(r.ok(),"duplicate setup");
            auto originalNode=meshNode(duplicate);auto clone=FbxNode::Create(duplicate.scene,"FaceCopy");clone->SetNodeAttribute(originalNode->GetMesh());duplicate.scene->GetRootNode()->AddChild(clone);
            Report check;validate(duplicate,db,check,1);bool found=false;for(auto& f:check.findings)if(f.code=="check.duplicate_shape")found=true;require(found,"duplicate blendshapes detected across meshes");
        }
        {
            Scene legacy;auto lb=fixture(legacy,root,false);lb.at("LeftArm")->SetName("Arm_L");lb.at("LeftInHandIndex")->SetName("HandIndex_L.0");lb.at("RightHandIndex1")->SetName("HandIndex_R1");
            Report r;auto resolved=resolve(legacy,r,"",false);require(r.ok()&&resolved.size()==73,"legacy names resolve without losing optional slots");
            renameBones(legacy,resolved,r);require(r.ok(),"legacy rename");require(std::string(resolved.at("LeftInHandIndex")->GetName())=="InHandIndex_L","legacy in-hand name normalized");
        }
        {
            Scene rename;auto nb=fixture(rename,root,false);Report setup;createCharacter(rename,nb,"Old_Miku",true,setup);
            nb.at("Spine")->SetName("OldSpine");nb.at("Spine1")->SetName("Spine1");nb.at("Spine2")->SetName("Spine2");
            rename.save(root/"rename.fbx");Report r;renameBones(rename,nb,r);require(r.ok(),"two phase renaming: "+r.json().dump());
            require(std::string(nb.at("Spine1")->GetName())=="Spine2","spine collision safely renamed");
        }
        std::cout<<"PASS "<<assertions<<" native assertions; fixtures: "<<root<<'\n';return 0;
    }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
