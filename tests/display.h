#pragma once

inline void testTexturedDisplay(const cmac::fs::path& root) {
    using namespace cmac;
    Scene source;auto bones=fixture(source,root/"display-source",false);
    auto face=meshNode(source);
    auto staticMesh=FbxNode::Create(source.scene,"StaticDisplayMesh");
    staticMesh->SetNodeAttribute(face->GetMesh());source.scene->GetRootNode()->AddChild(staticMesh);
    face->SetShadingMode(FbxNode::eHardShading);
    staticMesh->SetShadingMode(FbxNode::eWireFrame);
    bones.at("Reference")->SetShadingMode(FbxNode::eWireFrame);
    auto material=face->GetMaterial(0);auto geometry=face->GetMesh();
    auto point=geometry->GetControlPoints()[0];
    auto objects=source.scene->GetSrcObjectCount();Report report;
    applyTexturedDisplay(source,report);applyTexturedDisplay(source,report);
    require(report.ok() && source.scene->GetSrcObjectCount()==objects,"display changes are idempotent without extra scene objects");
    require(face->GetMaterial(0)==material && face->GetMesh()==geometry && near(geometry->GetControlPoints()[0],point),"display setting keeps materials and geometry");
    require(bones.at("Reference")->GetShadingMode()==FbxNode::eWireFrame,"non-mesh display mode is preserved");
    for(bool ascii:{false,true}) {
        auto path=root/(ascii?"display-ascii.fbx":"display-binary.fbx");source.save(path,ascii);
        Scene reopened;reopened.load(path);Report check;validateTexturedDisplay(reopened,check);
        require(check.ok(),"full shading survives FBX save and reload: "+check.json().dump());
        if(ascii) {
            std::ifstream input(path);std::string text((std::istreambuf_iterator<char>(input)),{});
            require(text.find("Shading: U")!=std::string::npos,"display uses the same serialized Shading: U as MotionBuilder references");
        }
        meshNode(reopened)->SetShadingMode(FbxNode::eTextureShading);
        Report wrong;validateTexturedDisplay(reopened,wrong);
        require(!wrong.ok(),"texture-only display does not pass full shading verification");
    }
}
