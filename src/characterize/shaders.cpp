#include "scene.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace cmac {
namespace {
constexpr const char* managedProperty = "CharacterizeOnMacFlatShader";
// MotionBuilder FlatShader version 250 and an empty relation block, identical in
// the supplied Gakupo, Chis-A and kiritan reference exports. These are serialized
// MotionBuilder metadata, not executable shader code. No user data is embedded.
constexpr unsigned char attributeData[] = {
    112,26,0,0,0,1,0,0,0,5,0,0,0,7,'V','e','r','s','i','o','n','I',250,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
constexpr unsigned char relationData[] = {112,0,0,0,0,0,0,0,0,0,0,0,0,0};

std::string stringProperty(FbxObject* object, const char* name) {
    auto p=object->FindProperty(name);
    return p.IsValid() && p.GetPropertyDataType().GetType()==eFbxString ? p.Get<FbxString>().Buffer() : "";
}
template<typename T>
void property(FbxObject* object, const char* name, const FbxDataType& type, const T& value, bool animated=false) {
    auto p=object->FindProperty(name);
    if(!p.IsValid())p=FbxProperty::Create(object,type,name);
    if(p.GetPropertyDataType().GetType()!=type.GetType() || !p.Set<T>(value))
        throw std::runtime_error("Flat シェーダーのプロパティを設定できません: "+std::string(name));
    if(animated)p.ModifyFlag(FbxPropertyFlags::eAnimatable,true);
}
bool blobMatches(FbxObject* object, const char* name, const unsigned char* bytes, int size) {
    auto p=object->FindProperty(name);
    if(!p.IsValid() || p.GetPropertyDataType().GetType()!=eFbxBlob)return false;
    auto blob=p.Get<FbxBlob>();
    return blob.Size()==size && std::memcmp(blob.Access(),bytes,size)==0;
}
template<typename T>
bool equals(FbxObject* object, const char* name, EFbxType type, const T& expected) {
    auto p=object->FindProperty(name);
    return p.IsValid() && p.GetPropertyDataType().GetType()==type && p.Get<T>()==expected;
}
bool flatSettings(FbxObject* shader) {
    return stringProperty(shader,"MoBuSubTypeName")=="FlatShader" &&
        equals(shader,"Enable",eFbxBool,true) && equals(shader,"Alpha",eFbxDouble,1.0) &&
        equals(shader,"ColorSource",eFbxEnum,FbxEnum(1)) &&
        equals(shader,"UseLuminosity",eFbxBool,false) &&
        equals(shader,"Transparency",eFbxEnum,FbxEnum(1)) &&
        blobMatches(shader,"MoBuAttrBlindData",attributeData,sizeof(attributeData)) &&
        blobMatches(shader,"MoBuRelationBlindData",relationData,sizeof(relationData));
}
}

bool isMotionBuilderShader(FbxObject* object) {
    auto type=object->GetRuntimeClassId().GetFbxFileTypeName();
    return type && std::string(type)=="MotionBuilder_Generic" && stringProperty(object,"MoBuTypeName")=="Shader";
}
std::vector<FbxObject*> modelShaders(FbxNode* node) {
    std::vector<FbxObject*> shaders;
    for(int i=0;i<node->GetSrcObjectCount();++i) {
        auto object=node->GetSrcObject(i);
        if(isMotionBuilderShader(object))shaders.push_back(object);
    }
    return shaders;
}
Json shaderInventory(FbxObject* shader) {
    Json result=Json::object();result["name"]=shader->GetName();
    result["type"]=stringProperty(shader,"MoBuSubTypeName");
    result["file_type"]=shader->GetRuntimeClassId().GetFbxFileTypeName();
    result["enabled"]=equals(shader,"Enable",eFbxBool,true);
    result["flat_settings_match"]=flatSettings(shader);
    return result;
}
void applyFlatShader(Scene& s, Report& report) {
    std::vector<FbxNode*> meshes;
    for(auto node:s.nodes())if(node->GetMesh())meshes.push_back(node);
    if(meshes.empty()) {
        report.add("error","shader.no_mesh","Flat シェーダーを適用するメッシュがありません。");
        return;
    }
    FbxObject* shader=nullptr;
    std::set<std::string> names;
    for(int i=0;i<s.scene->GetSrcObjectCount();++i) {
        auto object=s.scene->GetSrcObject(i);names.insert(object->GetName());
        if(!shader && isMotionBuilderShader(object) && equals(object,managedProperty,eFbxBool,true))shader=object;
    }
    if(!shader) {
        std::string name="Characterize_Flat";
        for(int suffix=1;names.count(name);++suffix)name="Characterize_Flat_"+std::to_string(suffix);
        auto cls=s.manager->FindFbxFileClass("MotionBuilder_Generic","");
        if(!cls.IsValid())cls=s.manager->RegisterRuntimeFbxClass("MotionBuilder_Generic_",FBX_TYPE(FbxObject),"MotionBuilder_Generic","");
        shader=s.manager->CreateNewObjectFromClassId(cls,name.c_str(),s.scene);
        if(!shader)throw std::runtime_error("MotionBuilder Flat シェーダーを作成できません。");
    }
    property(shader,managedProperty,FbxBoolDT,true);
    property(shader,"MoBuTypeName",FbxStringDT,FbxString("Shader"));
    property(shader,"MoBuSubTypeName",FbxStringDT,FbxString("FlatShader"));
    property(shader,"MoBuObjectFullName",FbxStringDT,FbxString(("Shader::"+std::string(shader->GetName())).c_str()));
    property(shader,"DefaultKeyingGroup",FbxIntDT,0);
    property(shader,"DefaultKeyingGroupEnum",FbxEnumDT,FbxEnum(0));
    property(shader,"Enable",FbxBoolDT,true,true);
    property(shader,"Alpha",FbxDoubleDT,1.0,true);
    property(shader,"UseLuminosity",FbxBoolDT,false);
    property(shader,"LuminosityFactor",FbxDoubleDT,0.0);
    property(shader,"ColorSource",FbxEnumDT,FbxEnum(1));
    property(shader,"SpecificColor",FbxColor3DT,FbxDouble3(1,1,1));
    property(shader,"Transparency",FbxEnumDT,FbxEnum(1));
    property(shader,"MoBuAttrBlindData",FbxBlobDT,FbxBlob(attributeData,sizeof(attributeData)));
    property(shader,"MoBuRelationBlindData",FbxBlobDT,FbxBlob(relationData,sizeof(relationData)));
    size_t disconnected=0;std::set<FbxSurfaceMaterial*> materials;
    for(auto node:meshes) {
        // MotionBuilder's Replace All operates on shader-to-model connections.
        // Keep material/texture connections and the old shader objects themselves.
        for(auto previous:modelShaders(node))if(previous!=shader) {
            if(!node->DisconnectSrcObject(previous))throw std::runtime_error("既存シェーダーを解除できません: "+nodePath(node));
            ++disconnected;
        }
        if(!node->IsConnectedSrcObject(shader) && !node->ConnectSrcObject(shader))
            throw std::runtime_error("Flat シェーダーを接続できません: "+nodePath(node));
        for(int i=0;i<node->GetMaterialCount();++i)materials.insert(node->GetMaterial(i));
    }
    Json op=Json::object();op["operation"]="flat_shader";op["shader"]=shader->GetName();
    op["meshes"]=meshes.size();op["materials"]=materials.size();op["replaced_connections"]=disconnected;
    report.operations.push(op);
    validateFlatShaders(s,report);
    report.add("info","shader.motionbuilder_unverified","参照 FBX と同形式の Flat シェーダーを設定しました。MotionBuilder での表示は未検証です。");
}
void validateFlatShaders(Scene& s, Report& report) {
    size_t meshes=0;
    for(auto node:s.nodes())if(node->GetMesh()) {
        ++meshes;auto shaders=modelShaders(node);
        if(shaders.size()!=1 || !flatSettings(shaders.front()))
            report.add("error","shader.not_flat","メッシュに有効な Flat シェーダーが1つ必要です: "+nodePath(node));
    }
    if(!meshes)report.add("error","shader.no_mesh","Flat シェーダーを検査するメッシュがありません。");
}
}
