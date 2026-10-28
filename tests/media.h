#pragma once

inline void testEmbeddedMedia(const cmac::fs::path& root) {
    using namespace cmac;
    auto input=root/"media-input",other=root/"media-other",cache=root/"media-cache";
    fs::create_directories(other);
    Scene s;fixture(s,input,false);Report setup;applyTextures(s,input,setup);require(setup.ok(),"embedded fixture setup");
    fs::copy_file(input/"Body.png",other/"Body.png",fs::copy_options::overwrite_existing);
    {std::ofstream different(other/"Body.png",std::ios::binary|std::ios::app);different<<'\n';}
    auto mat=static_cast<FbxSurfacePhong*>(meshNode(s)->GetMaterial(0));
    auto second=FbxFileTexture::Create(s.scene,"SecondImage");second->SetFileName((other/"Body.png").c_str());mat->NormalMap.ConnectSrcObject(second);
    auto shared=FbxFileTexture::Create(s.scene,"SharedImage");
    auto video=FbxVideo::Create(s.scene,"SharedVideo");video->SetFileName((input/"Body.png").c_str());shared->ConnectSrcObject(video);
    shared->SetFileName("/missing/old-path/Body.png");mat->Emissive.ConnectSrcObject(shared);
    fs::copy_file(input/"Body.png",input/"MeaningfulAtlas.png",fs::copy_options::overwrite_existing);
    auto atlas=FbxFileTexture::Create(s.scene,"MeaningfulAtlas");atlas->SetFileName((input/"MeaningfulAtlas.png").c_str());mat->Ambient.ConnectSrcObject(atlas);
    s.extractedMedia=input; // Imported shared media can retain old path aliases.
    Report preparation;auto expected=prepareEmbeddedTextures(s,cache,preparation);
    require(preparation.ok(),"prepare embedded media: "+preparation.json().dump());
    require(textureFile(s,second)!=textureFile(s,shared),"different images with same basename keep distinct stored names");
    require(textureFile(s,atlas).filename()=="MeaningfulAtlas.png","meaningful name survives even for duplicate content");
    require(textureFile(s,second).stem().string().find("Body_")==0,"collisions retain the semantic stem");
    s.save(root/"external-media.fbx");s.save(root/"embedded-media.fbx",false,true);
    auto isolated=root/"media-isolated";fs::create_directories(isolated);
    fs::copy_file(root/"embedded-media.fbx",isolated/"only.fbx",fs::copy_options::overwrite_existing);
    // Restore only generated fixture files even when an assertion throws.
    struct Hidden {
        std::vector<std::pair<fs::path,fs::path>> paths;
        void hide(const fs::path& p){auto hidden=p;hidden+="-hidden";fs::rename(p,hidden);paths.emplace_back(hidden,p);}
        ~Hidden(){for(auto it=paths.rbegin();it!=paths.rend();++it){std::error_code error;fs::rename(it->first,it->second,error);}}
    } hidden;
    hidden.hide(input);hidden.hide(other);hidden.hide(cache);
    auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto extraction=isolated/("extract-"+unique);
    Scene reopened;reopened.load(isolated/"only.fbx",extraction);Report check;
    validateEmbeddedTextures(reopened,extraction,check,&expected);
    require(check.ok(),"FBX alone restores all media with original images and preparation cache unavailable: "+check.json().dump());
    auto restored=static_cast<FbxSurfacePhong*>(meshNode(reopened)->GetMaterial(0));
    require(textureFile(reopened,restored->Ambient.GetSrcObject<FbxFileTexture>(0)).filename()=="MeaningfulAtlas.png","semantic filename survives standalone embedded roundtrip");
    require(restored->Diffuse.GetSrcObjectCount<FbxFileTexture>()==1 && restored->NormalMap.GetSrcObjectCount<FbxFileTexture>()==1 && restored->Emissive.GetSrcObjectCount<FbxFileTexture>()==1,"material channel connections preserved in single FBX");
    auto wrong=expected;wrong["MissingImage"].insert("wrong-content");Report mismatch;validateEmbeddedTextures(reopened,extraction,mismatch,&wrong);
    require(!mismatch.ok(),"missing or changed embedded payload rejected");
    Scene external;auto emptyExtraction=isolated/("external-extract-"+unique);external.load(root/"external-media.fbx",emptyExtraction);
    Report externalReport;validateEmbeddedTextures(external,emptyExtraction,externalReport);
    require(!externalReport.ok(),"external FBX cannot pass embedded verification");
    bool rejected=false;try{s.save(root/"unsupported-ascii.fbx",true,true);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"ASCII cannot silently omit embedded media");
    Scene missing;auto missingImage=FbxFileTexture::Create(missing.scene,"Missing");missingImage->SetFileName("/missing/image.png");
    Report missingReport;prepareEmbeddedTextures(missing,root/"missing-media-cache",missingReport);require(!missingReport.ok(),"missing embed source fails before export");
    Scene missingChannel;fixture(missingChannel,root,false);Report channelSetup;applyTextures(missingChannel,root,channelSetup);
    auto badTexture=FbxFileTexture::Create(missingChannel.scene,"MissingSpecular");badTexture->SetFileName("/missing/specular.png");
    static_cast<FbxSurfacePhong*>(meshNode(missingChannel)->GetMaterial(0))->Specular.ConnectSrcObject(badTexture);
    missingChannel.save(root/"missing-media.fbx");
    Scene sequence;auto sequenceTexture=FbxFileTexture::Create(sequence.scene,"Sequence");sequenceTexture->SetFileName((root/"Body.png").c_str());
    auto sequenceVideo=FbxVideo::Create(sequence.scene,"SequenceVideo");sequenceVideo->SetFileName((root/"Body.png").c_str());sequenceVideo->SetImageSequence(true);sequenceTexture->ConnectSrcObject(sequenceVideo);
    Report sequenceReport;prepareEmbeddedTextures(sequence,root/"sequence-media-cache",sequenceReport);require(!sequenceReport.ok(),"image sequences cannot silently skip embedding");
}

inline void testTextureMapping(const cmac::fs::path& root) {
    using namespace cmac;
    Scene source;auto bones=fixture(source,root/"mapping-source",false);Report setup;applyTextures(source,root/"mapping-source",setup);
    auto material=static_cast<FbxSurfacePhong*>(meshNode(source)->GetMaterial(0));
    auto texture=material->Diffuse.GetSrcObject<FbxFileTexture>(0);
    require(setup.ok() && texture->UVSet.Get()=="UVMap","directory textures select a real UV set");
    require(texture->GetMaterialUse()==FbxFileTexture::eDefaultMaterial,"material use matches MotionBuilder reference");
    require(texture->FindProperty("VideoProperty").GetSrcObject<FbxVideo>(0)==texture->GetSrcObject<FbxVideo>(0),"MotionBuilder VideoProperty points to the standard image media");
    texture->UVSet.Set(FbxString("missing-uv"));Report invalidUV;validate(source,bones,invalidUV,1);
    bool uvError=false;for(const auto& finding:invalidUV.findings)uvError|=finding.code=="check.texture_uv";
    require(uvError,"UV validation catches an existing image with an invalid mapping");texture->UVSet.Set(FbxString("UVMap"));
    texture->SetTranslation(0.2,0.3);texture->SetScale(2,3);texture->SetRotation(0,0,45);
    texture->SetWrapMode(FbxTexture::eClamp,FbxTexture::eRepeat);texture->SetSwapUV(true);
    texture->SetName("MeaningfulBodyTexture");source.save(root/"mapping-reference.fbx",false,true);
    Scene target;fixture(target,root/"mapping-target",false);
    {
        Scene reference;reference.load(root/"mapping-reference.fbx",root/"mapping-extracted");
        Report copied;applyReferenceTextures(target,reference,copied);
        require(copied.ok(),"embedded reference image resolution: "+copied.json().dump());
    }
    auto targetMaterial=static_cast<FbxSurfacePhong*>(meshNode(target)->GetMaterial(0));
    auto copied=targetMaterial->Diffuse.GetSrcObject<FbxFileTexture>(0);
    require(std::string(copied->GetName())=="MeaningfulBodyTexture" && copied->UVSet.Get()=="UVMap","reference name and UV selection preserved");
    require(copied->GetTranslationU()==0.2 && copied->GetTranslationV()==0.3 && copied->GetScaleU()==2 && copied->GetScaleV()==3 && copied->GetRotationW()==45,"reference texture transforms preserved");
    require(copied->GetWrapModeU()==FbxTexture::eClamp && copied->GetSwapUV() && copied->GetMaterialUse()==FbxFileTexture::eDefaultMaterial,"reference sampling settings preserved");
    require(fs::is_regular_file(textureFile(target,copied)),"copied image survives reference manager destruction");
    targetMaterial->TransparencyFactor.ConnectSrcObject(copied);
    auto video=FbxVideo::Create(target.scene,"OldVideo");video->SetFileName((root/"mapping-target/Body.png").c_str());
    auto nodeCount=target.nodes().size();auto diffuse=targetMaterial->Diffuse.Get();Report reset;resetTextures(target,reset);
    require(reset.ok() && target.scene->GetSrcObjectCount<FbxTexture>()==0 && target.scene->GetSrcObjectCount<FbxVideo>()==0,"reset removes stale textures and media");
    require(targetMaterial->TransparencyFactor.GetSrcObjectCount()==0,"reset disconnects Blender alpha inputs");
    require(target.nodes().size()==nodeCount && targetMaterial->Diffuse.Get()==diffuse,"reset keeps geometry and material values");
    Report reapplied;applyTextures(target,root/"mapping-target",reapplied);
    target.save(root/"mapping-reset.fbx",false,true);
    Scene reopened;reopened.load(root/"mapping-reset.fbx",root/"mapping-reset-extract");
    auto restored=meshNode(reopened)->GetMaterial(0)->FindProperty(FbxSurfaceMaterial::sDiffuse).GetSrcObject<FbxFileTexture>(0);
    require(restored!=nullptr,"reset and new diffuse connection survive export");
    auto restoredVideo=restored->FindProperty("VideoProperty").GetSrcObject<FbxVideo>(0);
    require(restoredVideo && restoredVideo==restored->GetSrcObject<FbxVideo>(0),"MotionBuilder property and standard video connections survive export");
    require(restored->UVSet.Get()=="UVMap" && restored->GetMaterialUse()==FbxFileTexture::eDefaultMaterial,"valid UV selection and material use survive embedded export");

    // One material can be shared by meshes with different UV set names.
    // In that case MotionBuilder must select the first set on each mesh.
    auto second=FbxNode::Create(target.scene,"SecondMesh");
    auto secondMesh=FbxMesh::Create(target.scene,"SecondMeshGeometry");
    second->SetNodeAttribute(secondMesh);target.scene->GetRootNode()->AddChild(second);
    second->AddMaterial(targetMaterial);secondMesh->CreateElementUV("UV1");
    Report shared;resetTextures(target,shared);applyTextures(target,root/"mapping-target",shared);
    auto sharedTexture=targetMaterial->Diffuse.GetSrcObject<FbxFileTexture>(0);
    require(shared.ok() && sharedTexture->UVSet.Get()=="","shared materials use each mesh's primary UV when names differ");
    Report sharedCheck;auto targetBones=resolve(target,sharedCheck,"",false);validate(target,targetBones,sharedCheck,1);
    bool sharedUVError=false;for(const auto& finding:sharedCheck.findings)sharedUVError|=finding.code=="check.texture_uv";
    require(!sharedUVError,"empty UV selection is valid across differently named primary sets");
    meshNode(target)->GetMesh()->GetElementUV(0)->SetName("default");
    secondMesh->GetElementUV(0)->SetName("default");
    Report namedDefault;resetTextures(target,namedDefault);applyTextures(target,root/"mapping-target",namedDefault);
    require(targetMaterial->Diffuse.GetSrcObject<FbxFileTexture>(0)->UVSet.Get()=="default","default is valid when it is an actual primary UV name");
}
