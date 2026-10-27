#pragma once

inline void testShaders(const cmac::fs::path& root) {
    using namespace cmac;
    {
        Scene empty;auto count=empty.scene->GetSrcObjectCount();Report report;applyFlatShader(empty,report);
        require(!report.ok() && empty.scene->GetSrcObjectCount()==count,"no mesh fails without creating a shader");
    }
    Scene s;auto bones=fixture(s,root,false);Report textures;applyTextures(s,root,textures);
    require(textures.ok(),"shader fixture textures");
    auto face=meshNode(s);auto body=static_cast<FbxSurfacePhong*>(face->GetMaterial(0));
    body->Diffuse.Set(FbxDouble3(0.2,0.4,0.6));body->TransparencyFactor.Set(0.35);
    body->Specular.Set(FbxDouble3(0.1,0.3,0.5));body->Shininess.Set(42);
    auto texture=body->Diffuse.GetSrcObject<FbxFileTexture>(0);
    auto detail=FbxSurfaceLambert::Create(s.scene,"Detail");detail->Diffuse.ConnectSrcObject(texture);face->AddMaterial(detail);
    auto unused=FbxSurfacePhong::Create(s.scene,"Unused");unused->Shininess.Set(17);
    auto staticMesh=FbxNode::Create(s.scene,"StaticMesh");s.scene->GetRootNode()->AddChild(staticMesh);
    auto geometry=FbxMesh::Create(s.scene,"StaticGeometry");staticMesh->SetNodeAttribute(geometry);
    geometry->InitControlPoints(3);geometry->SetControlPointAt(FbxVector4(0,0,0),0);
    geometry->SetControlPointAt(FbxVector4(1,0,0),1);geometry->SetControlPointAt(FbxVector4(0,1,0),2);
    geometry->BeginPolygon();for(int i=0;i<3;++i)geometry->AddPolygon(i);geometry->EndPolygon();
    staticMesh->AddMaterial(body); // A shared material on a non-skinned mesh.
    auto bare=FbxNode::Create(s.scene,"NoMaterial");bare->SetNodeAttribute(geometry);s.scene->GetRootNode()->AddChild(bare);
    auto metadata=FbxObject::Create(s.scene,"Characterize_Flat");face->ConnectSrcObject(metadata);
    auto cls=s.manager->RegisterRuntimeFbxClass("MotionBuilder_Generic_",FBX_TYPE(FbxObject),"MotionBuilder_Generic","");
    auto old=s.manager->CreateNewObjectFromClassId(cls,"ExistingLighted",s.scene);
    auto bump=s.manager->CreateNewObjectFromClassId(cls,"ExistingBump",s.scene);
    for(auto shader:{old,bump})FbxProperty::Create(shader,FbxStringDT,"MoBuTypeName").Set(FbxString("Shader"));
    FbxProperty::Create(old,FbxStringDT,"MoBuSubTypeName").Set(FbxString("LightedShader"));
    FbxProperty::Create(bump,FbxStringDT,"MoBuSubTypeName").Set(FbxString("BumpShader"));
    face->ConnectSrcObject(old);face->ConnectSrcObject(bump);staticMesh->ConnectSrcObject(old);
    bones.at("Reference")->ConnectSrcObject(old); // A connection outside the target meshes.
    s.save(root/"shaders-input.fbx");
    Report missing;validateFlatShaders(s,missing);require(!missing.ok(),"Lighted and missing shader connections rejected");
    auto sourceMaterial=body->ShadingModel.Get();
    body->ShadingModel.Set("Flat");Report fake;validateFlatShaders(s,fake);
    require(!fake.ok(),"a Flat material string does not count as a MotionBuilder shader");body->ShadingModel.Set(sourceMaterial);

    Report report;applyFlatShader(s,report);require(report.ok(),"flat shader application: "+report.json().dump());
    auto shader=modelShaders(face).at(0);
    require(std::string(shader->GetName())=="Characterize_Flat_1","name collision does not overwrite unrelated object");
    require(std::string(shader->GetRuntimeClassId().GetFbxFileTypeName())=="MotionBuilder_Generic","MotionBuilder file object type");
    require(std::string(shader->FindProperty("MoBuSubTypeName").Get<FbxString>())=="FlatShader","native FlatShader subtype");
    require(shader->FindProperty("Enable").Get<bool>() && shader->FindProperty("Alpha").Get<double>()==1,"enabled with full shader alpha");
    require(shader->FindProperty("ColorSource").Get<int>()==1,"uses material and texture colors");
    require(shader->FindProperty("Transparency").Get<int>()==1,"accurate transparency matches reference");
    require(shader->FindProperty("MoBuAttrBlindData").Get<FbxBlob>().Size()==39,"shader version block is present");
    for(auto node:{face,staticMesh,bare})require(modelShaders(node).size()==1 && modelShaders(node)[0]==shader,"one shared Flat shader per mesh");
    require(face->IsConnectedSrcObject(metadata),"unrelated model connections preserved");
    require(bones.at("Reference")->IsConnectedSrcObject(old),"shader users outside meshes preserved");
    require(old->GetDstObjectCount()>0 && bump->GetDstObjectCount()>0,"existing shader objects retained");
    require(face->GetMaterialCount()==2 && staticMesh->GetMaterial(0)==body,"material slots and shared material preserved");
    require(body->Diffuse.GetSrcObject<FbxFileTexture>(0)==texture && detail->Diffuse.GetSrcObject<FbxFileTexture>(0)==texture,"diffuse texture connections preserved");
    require(body->Diffuse.Get()==FbxDouble3(0.2,0.4,0.6) && body->TransparencyFactor.Get()==0.35,"material color and alpha preserved");
    require(body->Specular.Get()==FbxDouble3(0.1,0.3,0.5) && body->Shininess.Get()==42 && body->ShadingModel.Get()==sourceMaterial,"other material settings preserved");
    require(unused->Shininess.Get()==17,"unused material preserved");
    auto objectCount=s.scene->GetSrcObjectCount();Report again;applyFlatShader(s,again);
    require(again.ok() && s.scene->GetSrcObjectCount()==objectCount && modelShaders(face).at(0)==shader,"repeated application does not add shaders");
    for(bool ascii:{false,true}) {
        auto path=root/(ascii?"shaders-ascii.fbx":"shaders-binary.fbx");s.save(path,ascii);
        Scene reopened;reopened.load(path);Report check;validateFlatShaders(reopened,check);
        require(check.ok(),"shader metadata and connections survive export: "+check.json().dump());
        auto restored=modelShaders(meshNode(reopened)).at(0);
        require(restored->FindProperty("MoBuObjectFullName").Get<FbxString>()=="Shader::Characterize_Flat_1","full MotionBuilder object name survives export");
        Report reapplied;applyFlatShader(reopened,reapplied);
        require(reapplied.ok() && modelShaders(meshNode(reopened)).at(0)==restored,"reapply after reopen reuses shader");
        auto material=static_cast<FbxSurfacePhong*>(meshNode(reopened)->GetMaterial(0));
        require(material->Diffuse.Get()==FbxDouble3(0.2,0.4,0.6) && material->TransparencyFactor.Get()==0.35 && material->Shininess.Get()==42,"material values survive flat export");
    }
    shader->FindProperty("Enable").Set(false);Report disabled;validateFlatShaders(s,disabled);
    require(!disabled.ok(),"disabled flat shader rejected");shader->FindProperty("Enable").Set(true);
    face->ConnectSrcObject(old);Report extra;validateFlatShaders(s,extra);require(!extra.ok(),"additional lighting shader rejected");
    face->DisconnectSrcObject(old);shader->FindProperty("MoBuAttrBlindData").Set(FbxBlob());
    Report damaged;validateFlatShaders(s,damaged);require(!damaged.ok(),"missing serialized version rejected");
}
