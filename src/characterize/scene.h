#pragma once
#include <fbxsdk.h>
#include "json.h"
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cmac {
namespace fs = std::filesystem;
struct Slot { std::string name, bone; FbxCharacter::ENodeId id; bool required; };
const std::vector<Slot>& slots();
using Bones = std::map<std::string, FbxNode*>;
struct Finding { std::string severity, code, message; Json details; };
struct Report {
    std::vector<Finding> findings;
    Json operations = Json::array();
    void add(std::string severity, std::string code, std::string message, Json details = Json());
    bool ok() const;
    Json json() const;
};
struct Scene {
    FbxManager* manager = nullptr;
    FbxScene* scene = nullptr;
    fs::path source;
    fs::path extractedMedia;
    Scene();
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    void load(const fs::path& path, const fs::path& extractFolder = {});
    void save(const fs::path& path, bool ascii = false, bool embedTextures = false);
    std::vector<FbxNode*> nodes() const;
    Json inventory() const;
};
std::string leaf(const std::string& name);
std::string nameSpace(const std::string& name);
std::string nodePath(FbxNode* node);
Json vectorJson(const FbxVector4& v);
FbxAMatrix global(FbxNode* node);
bool isAncestor(FbxNode* ancestor, FbxNode* child);
Bones resolve(Scene& s, Report& report, const std::string& ns, bool useCharacter);
void renameBones(Scene& s, const Bones& bones, Report& report);
void createCharacter(Scene& s, const Bones& bones, const std::string& name, bool replace, Report& report);
void applyTextures(Scene& s, const fs::path& folder, Report& report);
void applyReferenceTextures(Scene& s, Scene& reference, Report& report);
void resetTextures(Scene& s, Report& report);
void applyTexturedDisplay(Scene& s, Report& report);
void validateTexturedDisplay(Scene& s, Report& report);
bool isMotionBuilderShader(FbxObject* object);
std::vector<FbxObject*> modelShaders(FbxNode* node);
Json shaderInventory(FbxObject* shader);
void applyFlatShader(Scene& s, Report& report);
void validateFlatShaders(Scene& s, Report& report);
using TextureManifest = std::map<std::string, std::multiset<std::string>>;
fs::path textureFile(Scene& s, FbxFileTexture* texture);
TextureManifest prepareEmbeddedTextures(Scene& s, const fs::path& folder, Report& report);
void validateEmbeddedTextures(Scene& s, const fs::path& extracted, Report& report, const TextureManifest* expected = nullptr);
void createGroups(Scene& s, const Bones& bones, const std::string& name, Report& report);
void validate(Scene& s, const Bones& bones, Report& report, double tolerance);
void validateSkeleton(const Bones& bones, Report& report);
Json poseAngles(const Bones& bones);
void checkPose(const Bones& bones, Report& report, double tolerance);
void correctPose(Scene& s, const Bones& bones, Report& report, double maxAngle);
bool validCharacterName(const std::string& name);
std::vector<FbxNode*> skinMeshes(Scene& s, const Bones& bones);
}
