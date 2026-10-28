#include "scene.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cmac {
namespace {
std::vector<fs::path> texturePaths(Scene& s, FbxFileTexture* texture) {
    std::vector<fs::path> paths;
    auto add=[&](const char* full,const char* relative) {
        if(full && *full)paths.emplace_back(full);
        if(relative && *relative)paths.push_back(s.source.parent_path()/relative);
    };
    // Several textures can share one embedded video. The imported video path
    // can be remapped to the extraction cache while a texture path stays old.
    for(int i=0;i<texture->GetSrcObjectCount<FbxVideo>();++i) {
        auto video=texture->GetSrcObject<FbxVideo>(i);
        add(video->GetFileName().Buffer(),video->GetRelativeFileName().Buffer());
    }
    add(texture->GetFileName(),texture->GetRelativeFileName());
    if(!s.extractedMedia.empty()) {
        auto original=paths;
        for(const auto& path:original) {
            auto name=path.string();std::replace(name.begin(),name.end(),'\\','/');
            paths.push_back(s.extractedMedia/fs::path(name).filename());
        }
    }
    return paths;
}
std::string digest(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("画像を読み込めません: "+path.string());
    uint64_t hash=14695981039346656037ULL;size_t size=0;char bytes[65536];
    while(stream) {
        stream.read(bytes,sizeof(bytes));size+=stream.gcount();
        for(std::streamsize i=0;i<stream.gcount();++i){hash^=static_cast<unsigned char>(bytes[i]);hash*=1099511628211ULL;}
    }
    if(!stream.eof())throw std::runtime_error("画像の読み込みに失敗しました: "+path.string());
    std::ostringstream out;out<<std::hex<<hash<<'-'<<size;return out.str();
}
bool inside(const fs::path& file,const fs::path& folder) {
    auto relative=fs::weakly_canonical(file).lexically_relative(fs::weakly_canonical(folder));
    return !relative.empty() && *relative.begin()!=".." && relative!=".";
}
}
fs::path textureFile(Scene& s,FbxFileTexture* texture) {
    for(const auto& path:texturePaths(s,texture))if(fs::is_regular_file(path))return fs::weakly_canonical(fs::absolute(path));
    return {};
}
TextureManifest prepareEmbeddedTextures(Scene& s,const fs::path& folder,Report& report) {
    struct Image {FbxFileTexture* texture;fs::path source,target;std::string hash;};
    std::vector<Image> images;TextureManifest manifest;std::set<std::string> unique;
    for(int i=0;i<s.scene->GetSrcObjectCount<FbxFileTexture>();++i) {
        auto texture=s.scene->GetSrcObject<FbxFileTexture>(i);auto path=textureFile(s,texture);
        if(path.empty()) {
            report.add("error","texture.embed_source","埋め込む画像が見つかりません: "+std::string(texture->GetName()));continue;
        }
        bool sequence=false;
        for(int j=0;j<texture->GetSrcObjectCount<FbxVideo>();++j)sequence|=texture->GetSrcObject<FbxVideo>(j)->GetImageSequence();
        if(sequence){report.add("error","texture.embed_sequence","画像シーケンスは FBX 単体へ埋め込めません: "+std::string(texture->GetName()));continue;}
        auto hash=digest(path);
        images.push_back({texture,path,fs::absolute(folder)/path.filename(),hash});
        manifest[texture->GetName()].insert(hash);unique.insert(hash);
    }
    if(!report.ok())return manifest;
    // Keep meaningful source names, even when two differently named images
    // contain identical bytes. Only disambiguate actual filename collisions.
    auto key=[](std::string name) {
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
        return name;
    };
    std::map<std::string,std::set<std::string>> contents;
    for(const auto& image:images)contents[key(image.target.filename().string())].insert(image.hash);
    std::set<std::string> reserved;
    for(const auto& entry:contents)reserved.insert(entry.first);
    std::map<std::pair<std::string,std::string>,fs::path> assigned;
    for(auto& image:images) {
        auto name=key(image.target.filename().string());
        if(contents[name].size()==1)continue;
        auto id=std::make_pair(name,image.hash);
        if(!assigned.count(id)) {
            auto stem=image.source.stem().string()+"_"+image.hash;
            auto candidate=stem+image.source.extension().string();
            for(int i=2;reserved.count(key(candidate));++i)candidate=stem+"_"+std::to_string(i)+image.source.extension().string();
            reserved.insert(key(candidate));assigned[id]=fs::absolute(folder)/candidate;
        }
        image.target=assigned[id];
    }
    if(!images.empty())fs::create_directories(folder);
    for(const auto& image:images) {
        if(!fs::exists(image.target) || digest(image.target)!=image.hash)
            fs::copy_file(image.source,image.target,fs::copy_options::overwrite_existing);
        auto relative=image.target.filename().string();
        for(int i=0;i<image.texture->GetSrcObjectCount<FbxVideo>();++i) {
            auto video=image.texture->GetSrcObject<FbxVideo>(i);
            video->SetFileName(image.target.c_str());video->SetRelativeFileName(relative.c_str());
        }
        image.texture->SetFileName(image.target.c_str());image.texture->SetRelativeFileName(relative.c_str());
    }
    Json op=Json::object();op["operation"]="embed_textures";op["textures"]=images.size();op["images"]=unique.size();report.operations.push(op);
    return manifest;
}
void validateEmbeddedTextures(Scene& s,const fs::path& extracted,Report& report,const TextureManifest* expected) {
    TextureManifest actual;size_t textures=0;std::set<std::string> unique;
    for(int i=0;i<s.scene->GetSrcObjectCount<FbxFileTexture>();++i) {
        auto texture=s.scene->GetSrcObject<FbxFileTexture>(i);fs::path embedded;
        // Source paths may still exist on this Mac. Accept only files newly
        // extracted from this FBX, never a successful external-path fallback.
        for(const auto& path:texturePaths(s,texture))if(fs::is_regular_file(path) && inside(path,extracted)){embedded=path;break;}
        if(embedded.empty()) {
            report.add("error","texture.not_embedded","FBX 内から画像を復元できません: "+std::string(texture->GetName()));continue;
        }
        auto hash=digest(embedded);actual[texture->GetName()].insert(hash);unique.insert(hash);++textures;
    }
    if(expected && actual!=*expected)report.add("error","texture.embed_mismatch","埋め込み前と FBX から復元した画像の内容・接続数が一致しません。");
    Json op=Json::object();op["operation"]="embedded_textures_verified";op["textures"]=textures;op["images"]=unique.size();report.operations.push(op);
}
}
