#include "scene.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace cmac {
const std::vector<Slot>& slots() {
    static const std::vector<Slot> value = {
#include "mapping.inc"
    };
    return value;
}
void Report::add(std::string severity, std::string code, std::string message, Json details) {
    findings.push_back({std::move(severity), std::move(code), std::move(message), std::move(details)});
}
bool Report::ok() const {
    return std::none_of(findings.begin(), findings.end(), [](const auto& f) {return f.severity == "error";});
}
Json Report::json() const {
    Json result = Json::object(), list = Json::array();
    for (const auto& f : findings) {
        Json item = Json::object(); item["severity"] = f.severity; item["code"] = f.code;
        item["message"] = f.message; item["details"] = f.details; list.push(item);
    }
    result["ok"] = ok(); result["findings"] = list; result["operations"] = operations;
    // The FBX SDK writes definitions; it does not execute MotionBuilder's HIK solver.
    result["motionbuilder_verified"] = false;
    return result;
}
Scene::Scene() {
    manager = FbxManager::Create();
    if (!manager) throw std::runtime_error("FBX SDK の初期化に失敗しました。");
    manager->SetIOSettings(FbxIOSettings::Create(manager, IOSROOT));
    scene = FbxScene::Create(manager, "Scene");
}
Scene::~Scene() { if (manager) manager->Destroy(); }
void Scene::load(const fs::path& path, const fs::path& extractFolder) {
    source = fs::absolute(path);
    extractedMedia = extractFolder.empty() ? fs::path() : fs::absolute(extractFolder);
    if (!fs::is_regular_file(source)) throw std::runtime_error("入力ファイルがありません: " + source.string());
    // The CLI supplies its ignored cache directory for embedded image extraction.
    // Never create an .fbm directory next to the source just by inspecting it.
    manager->GetIOSettings()->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, !extractFolder.empty());
    auto importer = FbxImporter::Create(manager, "");
    if(!extractFolder.empty()) {
        fs::create_directories(extractFolder);
        importer->SetEmbeddingExtractionFolder(fs::absolute(extractFolder).c_str());
    }
    if (!importer->Initialize(source.c_str(), -1, manager->GetIOSettings())) {
        std::string message = importer->GetStatus().GetErrorString(); importer->Destroy();
        throw std::runtime_error("FBX を開けません: " + message);
    }
    bool ok = importer->Import(scene);
    std::string message = importer->GetStatus().GetErrorString(); importer->Destroy();
    if (!ok) throw std::runtime_error("FBX の読み込みに失敗: " + message);
}
void Scene::save(const fs::path& path, bool ascii, bool embedTextures) {
    if(ascii && embedTextures)throw std::runtime_error("画像の埋め込みにはバイナリ FBX が必要です。");
    auto exporter = FbxExporter::Create(manager, "");
    int format = -1;
    for (int i=0; i<manager->GetIOPluginRegistry()->GetWriterFormatCount(); ++i) {
        if (!manager->GetIOPluginRegistry()->WriterIsFBX(i)) continue;
        std::string description = manager->GetIOPluginRegistry()->GetWriterFormatDescription(i);
        if (description.find(ascii ? "ascii" : "binary") != std::string::npos) {format = i; break;}
    }
    auto io = manager->GetIOSettings();
    io->SetBoolProp(EXP_FBX_CHARACTER, true); io->SetBoolProp(EXP_FBX_CONSTRAINT, true);
    io->SetBoolProp(EXP_FBX_EMBEDDED, embedTextures);
    if (!exporter->Initialize(path.c_str(), format, io)) {
        std::string message = exporter->GetStatus().GetErrorString(); exporter->Destroy();
        throw std::runtime_error("出力の初期化に失敗: " + message);
    }
    bool ok = exporter->Export(scene);
    std::string message = exporter->GetStatus().GetErrorString(); exporter->Destroy();
    if (!ok) throw std::runtime_error("FBX の保存に失敗: " + message);
}
std::vector<FbxNode*> Scene::nodes() const {
    std::vector<FbxNode*> out;
    std::function<void(FbxNode*)> walk = [&](FbxNode* n) {
        if (n != scene->GetRootNode()) out.push_back(n);
        for (int i=0; i<n->GetChildCount(); ++i) walk(n->GetChild(i));
    };
    walk(scene->GetRootNode()); return out;
}
std::string leaf(const std::string& name) {
    auto pos = name.rfind(':'); return pos == std::string::npos ? name : name.substr(pos+1);
}
std::string nameSpace(const std::string& name) {
    auto pos = name.rfind(':'); return pos == std::string::npos ? "" : name.substr(0,pos);
}
std::string nodePath(FbxNode* node) {
    if (!node) return "";
    std::string result = node->GetName();
    for (auto p=node->GetParent(); p && p->GetParent(); p=p->GetParent()) result=std::string(p->GetName())+"/"+result;
    return result;
}
Json vectorJson(const FbxVector4& v) {Json a=Json::array(); for (int i=0;i<3;++i) a.push(v[i]); return a;}
FbxAMatrix global(FbxNode* node) { return node->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE, FbxNode::eSourcePivot, false, true); }
bool isAncestor(FbxNode* ancestor, FbxNode* child) {
    for (auto p=child ? child->GetParent() : nullptr; p; p=p->GetParent()) if (p==ancestor) return true;
    return false;
}
static std::vector<std::string> shapeNames(FbxMesh* mesh) {
    std::vector<std::string> result;
    for (int d=0;d<mesh->GetDeformerCount(FbxDeformer::eBlendShape);++d) {
        auto blend=static_cast<FbxBlendShape*>(mesh->GetDeformer(d,FbxDeformer::eBlendShape));
        for (int c=0;c<blend->GetBlendShapeChannelCount();++c) {
            auto ch=blend->GetBlendShapeChannel(c); result.push_back(leaf(ch->GetName()));
        }
    }
    return result;
}
Json Scene::inventory() const {
    Json out=Json::object(), items=Json::array(), characters=Json::array(), meshes=Json::array(), mats=Json::array(), sets=Json::array();
    out["fbx_sdk"] = FBXSDK_VERSION_STRING;
    int sign=0, frontSign=0;
    auto axis=scene->GetGlobalSettings().GetAxisSystem();
    out["up_axis"] = int(axis.GetUpVector(sign)); out["up_sign"] = sign;
    out["front_parity"] = int(axis.GetFrontVector(frontSign)); out["front_sign"] = frontSign;
    out["right_handed"] = axis.GetCoorSystem()==FbxAxisSystem::eRightHanded;
    out["motionbuilder_axes"] = axis==FbxAxisSystem::Motionbuilder;
    out["centimeters_per_unit"] = scene->GetGlobalSettings().GetSystemUnit().GetScaleFactor();
    out["animation_stacks"] = scene->GetSrcObjectCount<FbxAnimStack>();
    out["character_poses"] = scene->GetCharacterPoseCount();
    for (auto n : nodes()) {
        Json item=Json::object(); item["name"]=n->GetName(); item["path"]=nodePath(n);
        item["parent"]=n->GetParent() ? n->GetParent()->GetName() : "";
        item["global_position"]=vectorJson(global(n).GetT());
        item["local_rotation"]=vectorJson(FbxVector4(n->LclRotation.Get()));
        item["local_scale"]=vectorJson(FbxVector4(n->LclScaling.Get()));
        item["global_scale"]=vectorJson(global(n).GetS());
        item["visible"]=n->Visibility.Get()>0 && n->Show.Get();
        item["skeleton"]=n->GetSkeleton()!=nullptr; items.push(item);
        if (auto mesh=n->GetMesh()) {
            Json m=Json::object(), shapes=Json::array(), materials=Json::array();
            m["name"]=n->GetName(); m["control_points"]=mesh->GetControlPointsCount(); m["polygons"]=mesh->GetPolygonCount();
            for (const auto& sh : shapeNames(mesh)) shapes.push(sh);
            for (int i=0;i<n->GetMaterialCount();++i) materials.push(n->GetMaterial(i)->GetName());
            m["shapes"]=shapes; m["materials"]=materials;
            Json shaders=Json::array();for(auto shader:modelShaders(n))shaders.push(shaderInventory(shader));
            m["shaders"]=shaders;
            m["skin_deformers"]=mesh->GetDeformerCount(FbxDeformer::eSkin); meshes.push(m);
        }
    }
    for (int i=0;i<scene->GetCharacterCount();++i) {
        auto ch=scene->GetCharacter(i); Json c=Json::object(), links=Json::object(), offsets=Json::object();
        c["name"]=ch->GetName(); c["characterize_flag"]=ch->Characterize.Get();
        for (const auto& sl : slots()) {FbxCharacterLink link; if(ch->GetCharacterLink(sl.id,&link) && link.mNode) {
            links[sl.name]=link.mNode->GetName(); Json offset=Json::object();
            offset["translation"]=vectorJson(link.mOffsetT); offset["rotation"]=vectorJson(link.mOffsetR); offset["scale"]=vectorJson(link.mOffsetS);
            offset["pre_rotation"]=vectorJson(link.mPreRotation);offset["post_rotation"]=vectorJson(link.mPostRotation);offset["has_rotation_space"]=link.mHasRotSpace;
            offsets[sl.name]=offset;
        }}
        c["links"]=links;c["link_details"]=offsets; characters.push(c);
    }
    for (int i=0;i<scene->GetSrcObjectCount<FbxSurfaceMaterial>();++i) {
        auto mat=scene->GetSrcObject<FbxSurfaceMaterial>(i); Json m=Json::object(), texs=Json::array(); m["name"]=mat->GetName();
        m["shading_model"]=mat->ShadingModel.Get().Buffer();
        auto prop=mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        for (int j=0;j<prop.GetSrcObjectCount<FbxFileTexture>();++j) {
            auto tex=prop.GetSrcObject<FbxFileTexture>(j); Json t=Json::object();
            t["file"]=tex->GetFileName(); t["relative_file"]=tex->GetRelativeFileName(); texs.push(t);
        }
        m["diffuse_textures"]=texs; mats.push(m);
    }
    for (int i=0;i<scene->GetSrcObjectCount<FbxSelectionSet>();++i) {
        auto set=scene->GetSrcObject<FbxSelectionSet>(i); Json x=Json::object(), members=Json::array(); x["name"]=set->GetName();
        for (int j=0;j<set->GetMemberCount();++j) members.push(set->GetMember(j)->GetName());
        x["members"]=members; sets.push(x);
    }
    out["nodes"]=items; out["characters"]=characters; out["meshes"]=meshes; out["materials"]=mats; out["groups"]=sets;
    Json collections=Json::array();
    for(int i=0;i<scene->GetSrcObjectCount<FbxCollection>();++i) {
        auto collection=scene->GetSrcObject<FbxCollection>(i);Json c=Json::object(),members=Json::array();
        c["name"]=collection->GetName();c["class"]=collection->GetClassId().GetName();
        for(int j=0;j<collection->GetMemberCount();++j)members.push(collection->GetMember(j)->GetName());
        c["members"]=members;collections.push(c);
    }
    out["collections"]=collections;
    Json shaders=Json::array();
    for(int i=0;i<scene->GetSrcObjectCount();++i) {
        auto object=scene->GetSrcObject(i);if(isMotionBuilderShader(object))shaders.push(shaderInventory(object));
    }
    out["shaders"]=shaders;
    return out;
}
bool validCharacterName(const std::string& name) {
    if (name.empty() || name=="1") return false;
    for (const auto& word : {"Character","character","chara"}) if(name.find(word)!=std::string::npos) return false;
    return name.find_first_of("\r\n\t/\\:")==std::string::npos && std::any_of(name.begin(),name.end(),[](unsigned char c){return !std::isspace(c);});
}
Bones resolve(Scene& s, Report& report, const std::string& ns, bool useCharacter) {
    Bones bones;
    auto nodes=s.nodes();
    if (useCharacter && s.scene->GetCharacterCount()!=1) {
        report.add("error","rename.character_count","既存 Character による改名には Character が1つ必要です。"); return bones;
    }
    for (const auto& sl : slots()) {
        if (useCharacter) {
            FbxCharacterLink link;
            if (s.scene->GetCharacter(0)->GetCharacterLink(sl.id,&link) && link.mNode) bones[sl.name]=link.mNode;
        } else {
            std::vector<FbxNode*> matches;
            for (auto n : nodes) {
                if (!ns.empty() && nameSpace(n->GetName())!=ns) continue;
                if (leaf(n->GetName())==sl.bone && !n->GetMesh()) matches.push_back(n);
            }
            // Explicit legacy names found in the supplied exports. Do not infer
            // numbered spine layouts or guess by suffix/substring.
            if(matches.empty()) {
                std::string alias;
                if(sl.name=="LeftArm")alias="Arm_L";
                else if(sl.name=="RightArm")alias="Arm_R";
                else if(sl.name.find("InHand")!=std::string::npos) {
                    bool left=sl.name.rfind("Left",0)==0;
                    auto prefix=std::string(left?"Left":"Right")+"InHand";
                    auto finger=sl.name.substr(prefix.size());
                    alias="Hand"+finger+(left?"_L.0":"_R.0");
                }else if(sl.bone.rfind("Hand",0)==0&&sl.bone.find('.')!=std::string::npos) {
                    alias=sl.bone;alias.erase(alias.find('.'),1);
                }
                if(!alias.empty()) for(auto n:nodes)
                    if(!n->GetMesh()&&leaf(n->GetName())==alias&&(ns.empty()||nameSpace(n->GetName())==ns))matches.push_back(n);
                if(matches.size()==1)report.add("info","mapping.legacy_name","旧命名を対応付けました: "+alias+" → "+sl.bone);
            }
            if (matches.size()==1) bones[sl.name]=matches[0];
            else if (matches.size()>1) report.add("error","mapping.ambiguous","同名ボーンが複数あります。--namespace で対象を指定してください: "+sl.bone);
        }
        if (sl.required && !bones.count(sl.name)) report.add("error","mapping.missing_required","必須ボーンがありません: "+sl.bone);
    }
    std::set<FbxNode*> seen;
    for (auto [slot,n] : bones) if (!seen.insert(n).second) report.add("error","mapping.duplicate_node","複数のスロットに同じボーンが登録されています: "+nodePath(n));
    return bones;
}
void renameBones(Scene& s, const Bones& bones, Report& report) {
    std::map<FbxNode*,std::string> planned;
    for (const auto& sl : slots()) if (bones.count(sl.name)) {
        auto n=bones.at(sl.name); auto ns=nameSpace(n->GetName());
        planned[n]=(ns.empty()?"":ns+":")+sl.bone;
    }
    std::map<std::string,FbxNode*> finalNames;
    for (auto n : s.nodes()) {
        std::string final=planned.count(n)?planned[n]:n->GetName();
        auto [it,added]=finalNames.emplace(final,n);
        if (!added && it->second!=n) report.add("error","rename.collision","改名後の名前が衝突します: "+final);
    }
    if (!report.ok()) return;
    Json changes=Json::array();
    for (auto [n,name] : planned) {
        if(name==n->GetName())continue;
        Json change=Json::object(); change["from"]=n->GetName(); change["to"]=name; changes.push(change);
        n->SetName(("__cmac_"+std::to_string(n->GetUniqueID())).c_str());
    }
    for (auto [n,name] : planned) n->SetName(name.c_str());
    Json op=Json::object(); op["rename"]=changes; report.operations.push(op);
}
void validateSkeleton(const Bones& bones, Report& report) {
    const std::vector<std::pair<std::string,std::string>> chains={
        {"Reference","Hips"},{"Hips","Spine"},{"Spine","Head"},
        {"Hips","LeftUpLeg"},{"LeftUpLeg","LeftLeg"},{"LeftLeg","LeftFoot"},
        {"Hips","RightUpLeg"},{"RightUpLeg","RightLeg"},{"RightLeg","RightFoot"},
        {"Spine","LeftArm"},{"LeftArm","LeftForeArm"},{"LeftForeArm","LeftHand"},
        {"Spine","RightArm"},{"RightArm","RightForeArm"},{"RightForeArm","RightHand"}};
    for (auto [parent,child] : chains) if(bones.count(parent)&&bones.count(child) && !isAncestor(bones.at(parent),bones.at(child)))
        report.add("error","mapping.hierarchy",parent+" の子孫に "+child+" がありません。");
}
static FbxCharacterLink makeLink(FbxNode* n) {
    FbxCharacterLink link; link.mNode=n; link.mTemplateName=leaf(n->GetName()).c_str();
    // Store the actual stance TRS, rather than assuming the incoming skeleton is axis aligned.
    auto m=global(n); link.mOffsetT=m.GetT(); link.mOffsetR=m.GetR(); link.mOffsetS=m.GetS();
    // Supplied MotionBuilder exports store global stance offsets with no separate
    // character-link rotation space. The node itself retains its pivots and pre/post rotations.
    link.mHasRotSpace=false;
    return link;
}
void createCharacter(Scene& s, const Bones& bones, const std::string& name, bool replace, Report& report) {
    if (!validCharacterName(name)) {report.add("error","character.name","Character名が不適切です。例: Vimii_Miku"); return;}
    if (s.scene->GetCharacterCount() && !replace) {report.add("error","character.exists","既存 Character があります。置き換える場合は --replace-character を指定してください。");return;}
    while(s.scene->GetCharacterCount()) s.scene->DestroyCharacter(s.scene->GetCharacterCount()-1);
    auto ch=s.scene->GetCharacter(s.scene->CreateCharacter(name.c_str()));
    if (!ch) throw std::runtime_error("Character を作成できません。");
    for (const auto& sl : slots()) if (bones.count(sl.name)) {
        auto n=bones.at(sl.name);
        if (!ch->SetCharacterLink(sl.id,makeLink(n))) throw std::runtime_error("スロットの登録に失敗: "+sl.name);
        n->Visibility.Set(1.0); n->Show.Set(true);
    }
    for (auto n:s.nodes()) if (n->GetSkeleton()) n->GetSkeleton()->Size.Set(1.0);
    ch->SetInput(FbxCharacter::eInputStancePose); ch->Characterize.Set(true);
    report.add("info","character.definition_written","FBX Character 定義とスタンスのオフセットを作成しました。MotionBuilder の HIK での有効化・リターゲットは別途確認が必要です。");
}
std::vector<FbxNode*> skinMeshes(Scene& s, const Bones& bones) {
    std::set<FbxNode*> linked; for(auto [slot,n]:bones) linked.insert(n);
    std::vector<FbxNode*> result;
    for(auto n:s.nodes()) if(auto mesh=n->GetMesh()) {
        bool match=false;
        for(int d=0;d<mesh->GetDeformerCount(FbxDeformer::eSkin);++d) {
            auto skin=static_cast<FbxSkin*>(mesh->GetDeformer(d,FbxDeformer::eSkin));
            for(int c=0;c<skin->GetClusterCount();++c) {
                auto link=skin->GetCluster(c)->GetLink();
                for(auto bone:linked) if(link && (link==bone||isAncestor(bone,link))) match=true;
            }
        }
        if(match) result.push_back(n);
    }
    return result;
}
void applyTextures(Scene& s, const fs::path& folder, Report& report) {
    if(!fs::is_directory(folder)) {report.add("error","texture.directory","テクスチャフォルダがありません: "+folder.string());return;}
    std::map<std::string,std::vector<fs::path>> images;
    for(auto& entry:fs::directory_iterator(folder)) if(entry.is_regular_file()) {
        auto ext=entry.path().extension().string(); std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return std::tolower(c);});
        if(std::set<std::string>{".png",".jpg",".jpeg",".tga",".tif",".tiff",".bmp",".exr",".dds"}.count(ext)) images[entry.path().stem().string()].push_back(fs::absolute(entry.path()));
    }
    std::vector<std::pair<FbxSurfaceMaterial*,fs::path>> planned;
    std::set<FbxSurfaceMaterial*> used;
    for(auto n:s.nodes()) for(int i=0;i<n->GetMaterialCount();++i) used.insert(n->GetMaterial(i));
    for(auto mat:used) {
        auto key=leaf(mat->GetName()); auto it=images.find(key);
        if(it==images.end()) {report.add("error","texture.missing","マテリアルと同名の画像がありません: "+key);continue;}
        if(it->second.size()!=1) {report.add("error","texture.ambiguous","同名の画像が複数あります: "+key);continue;}
        auto prop=mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if(!prop.IsValid()) {report.add("error","texture.no_diffuse","Diffuse プロパティがありません: "+key);continue;}
        planned.emplace_back(mat,it->second[0]);
    }
    if(!report.ok()) return;
    for(auto [mat,path]:planned) {
        auto prop=mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        prop.DisconnectAllSrcObject();
        auto tex=FbxFileTexture::Create(s.scene,(leaf(mat->GetName())+"_Diffuse").c_str());
        tex->SetFileName(path.c_str()); tex->SetTextureUse(FbxTexture::eStandard);
        tex->SetMappingType(FbxTexture::eUV); tex->SetMaterialUse(FbxFileTexture::eModelMaterial);
        prop.ConnectSrcObject(tex);
        Json op=Json::object(); op["material"]=mat->GetName(); op["diffuse_image"]=path.string(); report.operations.push(op);
    }
}
void applyReferenceTextures(Scene& s, Scene& reference, Report& report) {
    std::map<std::string,std::vector<FbxSurfaceMaterial*>> byName;
    for(int i=0;i<reference.scene->GetSrcObjectCount<FbxSurfaceMaterial>();++i) {
        auto m=reference.scene->GetSrcObject<FbxSurfaceMaterial>(i);byName[leaf(m->GetName())].push_back(m);
    }
    std::set<FbxSurfaceMaterial*> used;
    for(auto n:s.nodes())for(int i=0;i<n->GetMaterialCount();++i)used.insert(n->GetMaterial(i));
    std::vector<std::pair<FbxSurfaceMaterial*,fs::path>> planned;
    for(auto mat:used) {
        auto found=byName.find(leaf(mat->GetName()));
        if(found==byName.end()||found->second.size()!=1){report.add("error","texture.reference_material","参照 FBX のマテリアルがないか曖昧です: "+std::string(mat->GetName()));continue;}
        auto prop=found->second[0]->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if(prop.GetSrcObjectCount<FbxFileTexture>()!=1){report.add("error","texture.reference_diffuse","参照マテリアルに単一の Diffuse 画像がありません: "+std::string(mat->GetName()));continue;}
        auto tex=prop.GetSrcObject<FbxFileTexture>(0);fs::path path=tex->GetFileName(),rel=tex->GetRelativeFileName();
        if(!fs::is_regular_file(path)&&!rel.empty())path=reference.source.parent_path()/rel;
        if(!fs::is_regular_file(path)){report.add("error","texture.reference_file","参照 FBX の画像を取得できません: "+std::string(mat->GetName()));continue;}
        if(!mat->FindProperty(FbxSurfaceMaterial::sDiffuse).IsValid()){report.add("error","texture.no_diffuse","Diffuse がありません: "+std::string(mat->GetName()));continue;}
        planned.emplace_back(mat,fs::absolute(path));
    }
    if(!report.ok())return;
    for(auto [mat,path]:planned) {
        auto prop=mat->FindProperty(FbxSurfaceMaterial::sDiffuse);prop.DisconnectAllSrcObject();
        auto tex=FbxFileTexture::Create(s.scene,(leaf(mat->GetName())+"_Diffuse").c_str());tex->SetFileName(path.c_str());
        tex->SetMappingType(FbxTexture::eUV);tex->SetTextureUse(FbxTexture::eStandard);tex->SetMaterialUse(FbxFileTexture::eModelMaterial);prop.ConnectSrcObject(tex);
        Json op=Json::object();op["material"]=mat->GetName();op["reference_diffuse"]=path.string();report.operations.push(op);
    }
}
void createGroups(Scene& s, const Bones& bones, const std::string& name, Report& report) {
    for(auto kind:{"Skeleton","Mesh"}) {
        auto groupName=name+"/"+kind;
        for(int i=s.scene->GetSrcObjectCount<FbxSelectionSet>()-1;i>=0;--i) {
            auto set=s.scene->GetSrcObject<FbxSelectionSet>(i);
            if(groupName==set->GetName()) set->Destroy();
        }
        auto group=FbxSelectionSet::Create(s.scene,groupName.c_str());
        group->SelectionSetAnnotation.Set("Created by characterize-on-mac");
        if(std::string(kind)=="Skeleton") for(auto [slot,n]:bones) group->AddMember(n);
        else for(auto n:skinMeshes(s,bones)) group->AddMember(n);
    }
    report.add("info","groups.created","Skeleton / Mesh の FBX SelectionSet を作成しました。MotionBuilder でのグループ表示は互換性確認が必要です。");
}
static bool effectiveVisible(FbxNode* n) {
    for(auto p=n;p;p=p->GetParent()) if(p->Visibility.Get()<=0 || !p->Show.Get()) return false;
    return true;
}
static bool textureExists(Scene& s,FbxFileTexture* tex) {
    fs::path abs=tex->GetFileName(), rel=tex->GetRelativeFileName();
    return (!abs.empty()&&fs::is_regular_file(abs)) || (!rel.empty()&&fs::is_regular_file(s.source.parent_path()/rel));
}
void validate(Scene& s, const Bones& bones, Report& report, double tolerance) {
    auto count=s.scene->GetCharacterCount();
    if(count!=1) report.add("error","check.character_count","Character は1つ必要です。現在: "+std::to_string(count));
    if(count==1) {
        auto ch=s.scene->GetCharacter(0);
        if(!validCharacterName(ch->GetName())) report.add("error","check.character_name","Character名が不適切です。");
        if(!ch->Characterize.Get()) report.add("error","check.characterize_flag","Characterize フラグが有効ではありません。");
        for(const auto& sl:slots()) {
            FbxCharacterLink link; bool exists=ch->GetCharacterLink(sl.id,&link)&&link.mNode;
            if((sl.required||sl.name=="Reference")&&!exists) report.add("error","check.character_link","Character の必須スロットが空です: "+sl.name);
            if(exists) {
                if(leaf(link.mNode->GetName())!=sl.bone) report.add("error","check.bone_name","命名規則と一致しません: "+std::string(link.mNode->GetName())+" → "+sl.bone);
                if(!effectiveVisible(link.mNode)) report.add("error","check.visibility","非表示のボーンがあります: "+nodePath(link.mNode));
            }
        }
    }
    if(!bones.count("Reference")) report.add("error","check.reference","Reference (Parent) がありません。");
    validateSkeleton(bones,report);
    auto meshes=skinMeshes(s,bones);
    if(meshes.empty()) report.add("error","check.skin_mesh","この Character にスキン接続したメッシュがありません。");
    std::map<std::string,std::set<std::string>> shapes;
    std::set<FbxSurfaceMaterial*> materials;
    for(auto n:meshes) {
        for(auto& sh:shapeNames(n->GetMesh())) shapes[sh].insert(nodePath(n));
        for(int i=0;i<n->GetMaterialCount();++i) materials.insert(n->GetMaterial(i));
        if(n->GetMaterialCount()==0) report.add("error","check.material","マテリアルがありません: "+nodePath(n));
        if(n->GetMesh()->GetElementUVCount()==0) report.add("error","check.uv","UV がありません: "+nodePath(n));
    }
    for(auto& [shape,owners]:shapes) if(owners.size()>1) {
        Json list=Json::array(); for(auto& owner:owners) list.push(owner);
        report.add("error","check.duplicate_shape","複数メッシュに同名のシェイプキーがあります: "+shape,list);
    }
    for(auto vowel:{"a","i","u","o"}) if(!shapes.count(vowel)) report.add("error","check.vowel_shape","母音のシェイプキーがありません: "+std::string(vowel));
    if(!shapes.count("e")) report.add("info","check.optional_e","シェイプキー e はありません（任意項目）。");
    for(auto mat:materials) {
        auto prop=mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if(prop.GetSrcObjectCount<FbxLayeredTexture>()) report.add("warning","check.layered_texture","レイヤーテクスチャは手動の見た目確認が必要です: "+std::string(mat->GetName()));
        if(!prop.GetSrcObjectCount<FbxFileTexture>()&&!prop.GetSrcObjectCount<FbxLayeredTexture>()) report.add("error","check.texture","Diffuse テクスチャがありません: "+std::string(mat->GetName()));
        for(int i=0;i<prop.GetSrcObjectCount<FbxFileTexture>();++i) if(!textureExists(s,prop.GetSrcObject<FbxFileTexture>(i))) report.add("error","check.texture_file","画像ファイルが見つかりません: "+std::string(prop.GetSrcObject<FbxFileTexture>(i)->GetFileName()));
    }
    if(count==1) for(auto kind:{"Skeleton","Mesh"}) {
        auto name=std::string(s.scene->GetCharacter(0)->GetName())+"/"+kind;
        FbxSelectionSet* group=nullptr; int matches=0;
        for(int i=0;i<s.scene->GetSrcObjectCount<FbxSelectionSet>();++i) {
            auto set=s.scene->GetSrcObject<FbxSelectionSet>(i); if(name==set->GetName()){group=set;++matches;}
        }
        if(matches!=1) report.add("error","check.group","グループがないか複数存在します: "+name);
        else {
            std::set<FbxObject*> expected, actual;
            if(std::string(kind)=="Skeleton") for(auto [slot,n]:bones) expected.insert(n);
            else for(auto n:meshes) expected.insert(n);
            for(int i=0;i<group->GetMemberCount();++i) actual.insert(group->GetMember(i));
            if(expected!=actual) report.add("error","check.group_members","グループのメンバーが一致しません: "+name);
        }
    }
    checkPose(bones,report,tolerance);
    report.add("info","advice.finish","Flat / Lighted シェーダーの見た目と HIK Template の出力を確認してください。X-Ray / Texture+Shader の Viewer 設定はこの CLI の対象外です。");
}
}
