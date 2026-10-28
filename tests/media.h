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
