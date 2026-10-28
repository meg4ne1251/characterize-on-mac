#pragma once
#include <limits>

inline void testReviewRegressions(const cmac::fs::path& root) {
    using namespace cmac;
    auto folder=root/"review";
    {
        Scene s;auto bones=fixture(s,folder/"source",false);Report setup;
        pipeline(s,bones,folder/"source",setup);require(setup.ok(),"review fixture setup");
        auto texture=meshNode(s)->GetMaterial(0)->FindProperty(FbxSurfaceMaterial::sDiffuse).GetSrcObject<FbxFileTexture>(0);
        auto source=textureFile(s,texture);
        texture->SetFileName("/missing/Body.png");texture->SetRelativeFileName("missing/Body.png");
        s.extractedMedia=folder/"extracted";fs::create_directories(s.extractedMedia);
        auto extracted=s.extractedMedia/source.filename();
        fs::copy_file(source,extracted,fs::copy_options::overwrite_existing);
        Report check;validate(s,bones,check,1);
        require(check.ok(),"valid extracted alias must satisfy image validation: "+check.json().dump());
        // Embedded media is authoritative even if an older external source exists.
        {std::ofstream image(extracted,std::ios::binary);image<<"embedded image differs from external source";}
        texture->GetSrcObject<FbxVideo>(0)->SetFileName(source.c_str());
        // A relative texture alias can be remapped independently of the video's full path.
        texture->SetRelativeFileName(extracted.c_str());
        require(textureFile(s,texture)==fs::weakly_canonical(fs::absolute(extracted)),"extracted payload takes priority over an existing external video image");
        // This pair tests reuse of an FBX pathname after its embedded payload is removed.
        s.extractedMedia.clear();
        texture->SetFileName(source.c_str());texture->SetRelativeFileName(source.filename().c_str());
        s.save(root/"review-embedded.fbx",false,true);
        for(int i=0;i<texture->GetSrcObjectCount<FbxVideo>();++i) {
            auto video=texture->GetSrcObject<FbxVideo>(i);
            video->SetFileName("/missing/Body.png");video->SetRelativeFileName("missing/Body.png");
        }
        texture->SetFileName("/missing/Body.png");texture->SetRelativeFileName("missing/Body.png");
        s.save(root/"review-external-missing.fbx");
    }
    {
        Scene s;auto bones=fixture(s,folder/"scale",false);
        bones.at("Reference")->LclScaling.Set(FbxDouble3(0.01,0.01005,0.01));
        auto rotation=bones.at("LeftArm")->LclRotation.Get();
        Report check;correctPose(s,bones,check,75);
        bool scaleError=false;for(const auto& f:check.findings)scaleError|=f.code=="pose.scale";
        require(scaleError,"small nonuniform scales must be rejected using relative tolerance");
        require(bones.at("LeftArm")->LclRotation.Get()==rotation,"unsupported scale is rejected before mutation");
        bones.at("Reference")->LclScaling.Set(FbxDouble3(0.01,0.01000001,0.01));
        s.scene->GetAnimationEvaluator()->Reset();Report noise;correctPose(s,bones,noise,75);
        require(noise.ok(),"small uniform scale with float noise remains supported: "+noise.json().dump());
    }
    for(double value:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        Scene s;auto bones=fixture(s,folder/"nonfinite",false);
        bones.at("LeftHand")->LclTranslation.Set(FbxDouble3(value,0,0));
        s.scene->GetAnimationEvaluator()->Reset();Report check;checkPose(bones,check,1);
        require(!check.ok(),"nonfinite limb positions must fail pose validation");
    }
}
