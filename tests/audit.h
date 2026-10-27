#pragma once
#include "scene.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>

// Independent, read-only audit of data that pose correction must preserve.
inline cmac::Json auditFiles(const cmac::fs::path& source,const cmac::fs::path& output,bool* passed=nullptr) {
    using namespace cmac;
    Scene before,after;before.load(source);after.load(output);Report report;size_t vertices=0,targets=0,weights=0;
    Report mappingReport;auto sourceBones=resolve(before,mappingReport,"",before.scene->GetCharacterCount()==1);
    std::map<std::string,std::string> renamed;
    for(auto& sl:slots())if(sourceBones.count(sl.name)) {
        auto original=std::string(sourceBones.at(sl.name)->GetName());auto ns=nameSpace(original);
        renamed[original]=(ns.empty()?"":ns+":")+sl.bone;
    }
    auto outputBones=resolve(after,mappingReport,"",true);
    double maxLengthError=0;
    for(auto side:{"Left","Right"})for(auto pair:std::vector<std::pair<std::string,std::string>>{{"Arm","ForeArm"},{"ForeArm","Hand"},{"UpLeg","Leg"},{"Leg","Foot"}}) {
        auto a=std::string(side)+pair.first,b=std::string(side)+pair.second;
        if(sourceBones.count(a)&&sourceBones.count(b)&&outputBones.count(a)&&outputBones.count(b)) {
            double oldLength=(global(sourceBones.at(a)).GetT()-global(sourceBones.at(b)).GetT()).Length();
            double newLength=(global(outputBones.at(a)).GetT()-global(outputBones.at(b)).GetT()).Length();
            double error=std::abs(oldLength-newLength)/std::max(oldLength,1e-9);maxLengthError=std::max(maxLengthError,error);
            if(error>1e-4)report.add("error","preservation.bone_length",a+": ボーン長が変化しました。");
        }
    }
    auto fail=[&](const std::string& name,const std::string& what){report.add("error","preservation."+what,name+": "+what);};
    auto close=[](double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-7*std::max({1.0,std::abs(a),std::abs(b)});};
    auto matrix=[&](const FbxAMatrix& a,const FbxAMatrix& b){for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(!close(a.Get(r,c),b.Get(r,c)))return false;return true;};
    auto nodesBefore=before.nodes(),nodesAfter=after.nodes();
    std::map<std::string,FbxNode*> meshesBefore,meshesAfter;
    for(auto n:nodesBefore)if(n->GetMesh())meshesBefore[nodePath(n)]=n;
    for(auto n:nodesAfter)if(n->GetMesh())meshesAfter[nodePath(n)]=n;
    if(meshesBefore.size()!=meshesAfter.size())fail("scene","mesh_count");
    if(nodesBefore.size()!=nodesAfter.size())fail("scene","node_count");
    for(auto& [name,node]:meshesBefore) {
        if(!meshesAfter.count(name)){fail(name,"missing_mesh");continue;}
        auto an=meshesAfter.at(name);auto b=node->GetMesh(),a=an->GetMesh();
        if(b->GetControlPointsCount()!=a->GetControlPointsCount())fail(name,"vertex_count");
        else for(int i=0;i<b->GetControlPointsCount();++i){++vertices;for(int axis=0;axis<3;++axis)if(!close(b->GetControlPoints()[i][axis],a->GetControlPoints()[i][axis]))fail(name,"vertex");}
        if(b->GetPolygonCount()!=a->GetPolygonCount()||b->GetPolygonVertexCount()!=a->GetPolygonVertexCount())fail(name,"polygon_count");
        else for(int i=0;i<b->GetPolygonVertexCount();++i)if(b->GetPolygonVertices()[i]!=a->GetPolygonVertices()[i])fail(name,"polygon_index");
        if(b->GetElementUVCount()!=a->GetElementUVCount())fail(name,"uv_count");
        else for(int i=0;i<b->GetElementUVCount();++i){auto bu=b->GetElementUV(i),au=a->GetElementUV(i);
            if(bu->GetDirectArray().GetCount()!=au->GetDirectArray().GetCount()||bu->GetIndexArray().GetCount()!=au->GetIndexArray().GetCount())fail(name,"uv_array_count");
            else {
                for(int j=0;j<bu->GetDirectArray().GetCount();++j)for(int axis=0;axis<2;++axis)if(!close(bu->GetDirectArray().GetAt(j)[axis],au->GetDirectArray().GetAt(j)[axis]))fail(name,"uv_value");
                for(int j=0;j<bu->GetIndexArray().GetCount();++j)if(bu->GetIndexArray().GetAt(j)!=au->GetIndexArray().GetAt(j))fail(name,"uv_index");
            }
        }
        if(b->GetElementNormalCount()!=a->GetElementNormalCount())fail(name,"normal_count");
        else for(int i=0;i<b->GetElementNormalCount();++i){auto bn=b->GetElementNormal(i),anorm=a->GetElementNormal(i);
            if(bn->GetDirectArray().GetCount()!=anorm->GetDirectArray().GetCount())fail(name,"normal_array_count");
            else for(int j=0;j<bn->GetDirectArray().GetCount();++j)for(int axis=0;axis<3;++axis)if(!close(bn->GetDirectArray().GetAt(j)[axis],anorm->GetDirectArray().GetAt(j)[axis]))fail(name,"normal_value");
        }
        if(node->GetMaterialCount()!=an->GetMaterialCount())fail(name,"material_count");
        else for(int i=0;i<node->GetMaterialCount();++i)if(std::string(node->GetMaterial(i)->GetName())!=an->GetMaterial(i)->GetName())fail(name,"material_name");
        if(b->GetDeformerCount(FbxDeformer::eBlendShape)!=a->GetDeformerCount(FbxDeformer::eBlendShape))fail(name,"blendshape_count");
        else for(int d=0;d<b->GetDeformerCount(FbxDeformer::eBlendShape);++d){
            auto bs=static_cast<FbxBlendShape*>(b->GetDeformer(d,FbxDeformer::eBlendShape)),as=static_cast<FbxBlendShape*>(a->GetDeformer(d,FbxDeformer::eBlendShape));
            if(bs->GetBlendShapeChannelCount()!=as->GetBlendShapeChannelCount()){fail(name,"shape_channel_count");continue;}
            for(int c=0;c<bs->GetBlendShapeChannelCount();++c){
                auto bc=bs->GetBlendShapeChannel(c),ac=as->GetBlendShapeChannel(c);
                if(std::string(bc->GetName())!=ac->GetName()||bc->GetTargetShapeCount()!=ac->GetTargetShapeCount()){fail(name,"shape_channel");continue;}
                for(int t=0;t<bc->GetTargetShapeCount();++t){auto bt=bc->GetTargetShape(t),at=ac->GetTargetShape(t);++targets;
                    if(bt->GetControlPointsCount()!=at->GetControlPointsCount()){fail(name,"shape_vertex_count");continue;}
                    for(int i=0;i<bt->GetControlPointsCount();++i)for(int axis=0;axis<3;++axis)if(!close(bt->GetControlPoints()[i][axis],at->GetControlPoints()[i][axis]))fail(name,"shape_vertex");
                }
            }
        }
        if(b->GetDeformerCount(FbxDeformer::eSkin)!=a->GetDeformerCount(FbxDeformer::eSkin))fail(name,"skin_count");
        else for(int d=0;d<b->GetDeformerCount(FbxDeformer::eSkin);++d){
            auto bs=static_cast<FbxSkin*>(b->GetDeformer(d,FbxDeformer::eSkin)),as=static_cast<FbxSkin*>(a->GetDeformer(d,FbxDeformer::eSkin));
            if(bs->GetClusterCount()!=as->GetClusterCount()){fail(name,"cluster_count");continue;}
            for(int c=0;c<bs->GetClusterCount();++c){auto bc=bs->GetCluster(c),ac=as->GetCluster(c);
                if(!bc->GetLink()||!ac->GetLink())fail(name,"cluster_bone");
                else {
                    std::string expected=bc->GetLink()->GetName();if(renamed.count(expected))expected=renamed.at(expected);
                    if(expected!=ac->GetLink()->GetName())fail(name,"cluster_bone");
                }
                FbxAMatrix bm,am;bc->GetTransformMatrix(bm);ac->GetTransformMatrix(am);if(!matrix(bm,am))fail(name,"bind_mesh_matrix");
                bc->GetTransformLinkMatrix(bm);ac->GetTransformLinkMatrix(am);if(!matrix(bm,am))fail(name,"bind_bone_matrix");
                if(bc->GetControlPointIndicesCount()!=ac->GetControlPointIndicesCount()){fail(name,"weight_count");continue;}
                for(int i=0;i<bc->GetControlPointIndicesCount();++i){++weights;if(bc->GetControlPointIndices()[i]!=ac->GetControlPointIndices()[i]||!close(bc->GetControlPointWeights()[i],ac->GetControlPointWeights()[i]))fail(name,"weight");}
            }
        }
    }
    if(passed)*passed=report.ok();
    auto result=report.json();result["vertices_checked"]=vertices;result["shape_targets_checked"]=targets;result["weights_checked"]=weights;result["max_bone_length_relative_error"]=maxLengthError;return result;
}
