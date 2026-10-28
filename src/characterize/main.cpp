#include "scene.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

using namespace cmac;
namespace {
struct Options {
    std::string command, name, ns;
    fs::path input,output,textures,textureReference,report;
    bool correct=false,rename=false,replace=false,force=false,allowErrors=false,flat=false,requireFlat=false;
    bool embed=true,storageOption=false,requireEmbedded=false,reset=false;
    double tolerance=1.0,maxAngle=75.0;
};
void help() {
    std::cout << R"(characterize-on-mac — macOS / Autodesk FBX SDK

  characterize inspect INPUT.fbx [--report report.json]
  characterize check INPUT.fbx [--report report.json] [--tolerance 1]
      [--require-flat-shader] [--require-embedded-textures]
  characterize configure INPUT.fbx --output OUTPUT.fbx --name Vimii_Miku
      [--textures DIRECTORY] [--correct-tpose] [--rename-from-character]
      [--texture-reference PROCESSED.fbx]
      [--reset-textures]
      [--flat-shader]
      [--embed-textures | --external-textures]
      [--replace-character] [--namespace NAME] [--report report.json]
      [--tolerance 1] [--max-correction-angle 75] [--allow-check-errors] [--force]

結果は JSON で標準出力にも出力します。終了コード: 0=成功、2=チェック不合格、1=実行エラー。
入力は変更しません。既存の出力を置換する場合だけ --force を指定してください。
画像は標準で FBX に埋め込みます。--external-textures の場合は .assets も同梱してください。
FBX 定義の作成と MotionBuilder の HIK ソルバ検証は別です。
)";
}
Options parse(int argc,char** argv) {
    if(argc<2||std::string(argv[1])=="--help"||std::string(argv[1])=="-h"){help();std::exit(0);}
    if(argc<3) throw std::runtime_error("入力 FBX を指定してください。--help で使用方法を表示します。");
    Options o;o.command=argv[1];o.input=argv[2];
    if(o.command!="inspect"&&o.command!="check"&&o.command!="configure") throw std::runtime_error("不明なコマンドです: "+o.command);
    for(int i=3;i<argc;++i) {
        std::string a=argv[i];
        auto value=[&]() -> std::string {if(i+1>=argc)throw std::runtime_error("値が必要です: "+a);return argv[++i];};
        if(a=="--name")o.name=value();else if(a=="--output")o.output=value();
        else if(a=="--textures")o.textures=value();else if(a=="--texture-reference")o.textureReference=value();else if(a=="--report")o.report=value();
        else if(a=="--namespace")o.ns=value();else if(a=="--correct-tpose")o.correct=true;
        else if(a=="--flat-shader")o.flat=true;else if(a=="--require-flat-shader")o.requireFlat=true;
        else if(a=="--require-embedded-textures")o.requireEmbedded=true;
        else if(a=="--reset-textures")o.reset=true;
        else if(a=="--embed-textures"||a=="--external-textures") {
            if(o.storageOption)throw std::runtime_error("画像の保存方式は1つ指定してください。");
            o.embed=a=="--embed-textures";o.storageOption=true;
        }
        else if(a=="--rename-from-character")o.rename=true;else if(a=="--replace-character")o.replace=true;
        else if(a=="--force")o.force=true;else if(a=="--allow-check-errors")o.allowErrors=true;
        else if(a=="--tolerance"||a=="--max-correction-angle") {
            auto v=value();size_t used=0;double d=std::stod(v,&used);
            if(used!=v.size()||!std::isfinite(d)||d<=0||d>=90)throw std::runtime_error("角度は0より大きく90未満で指定してください。");
            if(a=="--tolerance")o.tolerance=d;else o.maxAngle=d;
        }else throw std::runtime_error("不明なオプションです: "+a);
    }
    if(o.command=="configure"&&(o.output.empty()||o.name.empty()))throw std::runtime_error("configure には --output と --name が必要です。");
    if(!o.textures.empty()&&!o.textureReference.empty())throw std::runtime_error("--textures と --texture-reference はどちらか一方を指定してください。");
    if(o.reset&&o.textures.empty()&&o.textureReference.empty())throw std::runtime_error("--reset-textures には --textures または --texture-reference が必要です。");
    if(o.requireFlat&&o.command!="check")throw std::runtime_error("--require-flat-shader は check で使用してください。");
    if(o.requireEmbedded&&o.command!="check")throw std::runtime_error("--require-embedded-textures は check で使用してください。");
    if(o.command!="configure"&&(!o.output.empty()||!o.name.empty()||!o.textures.empty()||!o.textureReference.empty()||o.correct||o.rename||o.replace||o.allowErrors||o.flat||o.storageOption||o.reset))
        throw std::runtime_error("変更用オプションは configure で使用してください。");
    return o;
}
fs::path normalized(const fs::path& p){return fs::weakly_canonical(fs::absolute(p));}
fs::path cacheFor(const fs::path& p) {
    auto key=std::hash<std::string>{}(normalized(p).string());
    return fs::absolute("tmp/embedded")/(p.stem().string()+"-"+std::to_string(key));
}
void writable(const fs::path& path,const Options& o) {
    if(path.empty())return;
    if(normalized(path)==normalized(o.input))throw std::runtime_error("入力ファイルへの上書きは禁止しています。");
    if(fs::exists(path)&&!o.force)throw std::runtime_error("既に存在します。別の出力先か --force を指定してください: "+path.string());
    if(!fs::is_directory(fs::absolute(path).parent_path()))throw std::runtime_error("出力ディレクトリがありません: "+path.parent_path().string());
}
void writeReport(const fs::path& path,const Json& result) {
    if(path.empty())return;
    auto temp=path;temp+=".tmp";
    std::ofstream stream(temp);if(!stream)throw std::runtime_error("レポートを保存できません: "+path.string());
    stream<<result.dump()<<'\n';stream.close();if(!stream)throw std::runtime_error("レポートの書き込みに失敗しました。");
    fs::rename(temp,path);
}
void rebaseTextures(Scene& s,const fs::path& output) {
    for(int i=0;i<s.scene->GetSrcObjectCount<FbxFileTexture>();++i) {
        auto tex=s.scene->GetSrcObject<FbxFileTexture>(i);
        fs::path file=tex->GetFileName(),relative=tex->GetRelativeFileName();
        if(file.empty()||!fs::is_regular_file(file)) {
            if(!relative.empty()&&fs::is_regular_file(s.source.parent_path()/relative))file=s.source.parent_path()/relative;
            else continue;
        }
        file=fs::absolute(file);tex->SetFileName(file.c_str());
        tex->SetRelativeFileName(file.lexically_relative(fs::absolute(output).parent_path()).c_str());
    }
}
std::string contentKey(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("画像を読めません: "+path.string());
    uint64_t hash=14695981039346656037ULL;char buffer[65536];
    while(in) {in.read(buffer,sizeof(buffer));for(std::streamsize i=0;i<in.gcount();++i){hash^=static_cast<unsigned char>(buffer[i]);hash*=1099511628211ULL;}}
    if(!in.eof())throw std::runtime_error("画像の読み込みに失敗しました: "+path.string());
    std::ostringstream out;out<<std::hex<<hash;return out.str();
}
void packageTextures(Scene& s,const fs::path& output,Report& report) {
    auto folder=fs::absolute(output).parent_path()/(output.stem().string()+".assets");
    std::map<fs::path,fs::path> copied;
    for(int i=0;i<s.scene->GetSrcObjectCount<FbxFileTexture>();++i) {
        auto tex=s.scene->GetSrcObject<FbxFileTexture>(i);auto file=textureFile(s,tex);
        if(file.empty())continue;
        file=normalized(file);
        if(!copied.count(file)) {
            fs::create_directories(folder);auto target=folder/(contentKey(file)+file.extension().string());
            if(!fs::exists(target))fs::copy_file(file,target);
            copied[file]=target;
        }
        tex->SetFileName(copied.at(file).c_str());
        auto relative=copied.at(file).lexically_relative(fs::absolute(output).parent_path());
        tex->SetRelativeFileName(relative.c_str());
        // Keep the explicit MotionBuilder video connections on the packaged
        // image too; an old absolute video path must not override the texture.
        for(int j=0;j<tex->GetSrcObjectCount<FbxVideo>();++j) {
            auto video=tex->GetSrcObject<FbxVideo>(j);
            video->SetFileName(copied.at(file).c_str());video->SetRelativeFileName(relative.c_str());
        }
    }
    Json op=Json::object();op["packaged_images"]=copied.size();op["folder"]=folder.string();report.operations.push(op);
}
}
int main(int argc,char** argv) {
    fs::path staging;
    try {
        auto o=parse(argc,argv);writable(o.report,o);
        if(!o.output.empty()) {
            writable(o.output,o);
            if(normalized(o.output)==normalized(o.report.empty()?o.input:o.report))throw std::runtime_error("FBX とレポートには別々の出力先を指定してください。");
            if(o.output.extension()!=".fbx")throw std::runtime_error("出力には .fbx 拡張子を指定してください。");
        }
        auto extraction=cacheFor(o.input);
        if(o.requireEmbedded||o.command=="configure")extraction+="-verify-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        Scene s;s.load(o.input,extraction);Report report;
        bool useExisting=o.command!="configure"&&s.scene->GetCharacterCount()==1;
        Bones bones;
        bool wrote=false;
        if(o.command=="inspect") {
            bones=resolve(s,report,o.ns,useExisting);
        }else if(o.command=="check") {
            bones=resolve(s,report,o.ns,useExisting);
            if(s.scene->GetGlobalSettings().GetAxisSystem()!=FbxAxisSystem::Motionbuilder)report.add("error","scene.axes","MotionBuilder の軸規約ではありません。configure で軸変換してください。");
            validate(s,bones,report,o.tolerance);
            if(o.requireFlat)validateFlatShaders(s,report);
            if(o.requireEmbedded)validateEmbeddedTextures(s,extraction,report);
        }else {
            if(!validCharacterName(o.name))report.add("error","character.name","Character名が不適切です。例: Vimii_Miku");
            if(s.scene->GetCharacterCount()&&!o.replace)report.add("error","character.exists","既存 Character の置換には --replace-character が必要です。");
            // FBX SDK axis conversion preserves the mesh and bind transforms consistently.
            if(s.scene->GetGlobalSettings().GetAxisSystem()!=FbxAxisSystem::Motionbuilder) {
                FbxAxisSystem::Motionbuilder.ConvertScene(s.scene);
                report.add("info","scene.axes_converted","MotionBuilder の座標系へ変換しました。");
            }
            bones=resolve(s,report,o.ns,o.rename);validateSkeleton(bones,report);
            if(!bones.count("Reference"))report.add("error","mapping.missing_reference","サークルの規約に必要な Parent がありません。");
            if(report.ok())renameBones(s,bones,report);
            if(report.ok()&&o.correct)correctPose(s,bones,report,o.maxAngle);
            if(report.ok())checkPose(bones,report,o.tolerance);
            if(report.ok()&&o.reset)resetTextures(s,report);
            if(report.ok()&&!o.textures.empty())applyTextures(s,o.textures,report);
            if(report.ok()&&!o.textureReference.empty()) {
                Scene reference;reference.load(o.textureReference,cacheFor(o.textureReference));applyReferenceTextures(s,reference,report);
            }
            if(report.ok())createCharacter(s,bones,o.name,o.replace,report);
            if(report.ok())createGroups(s,bones,o.name,report);
            if(report.ok()&&o.flat)applyFlatShader(s,report);
            if(report.ok()) {
                validate(s,bones,report,o.tolerance);
                if(report.ok()||o.allowErrors) {
                    staging=o.output.parent_path()/("."+o.output.stem().string()+"."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp.fbx");
                    TextureManifest embedded;Report media;
                    if(o.embed) {
                        embedded=prepareEmbeddedTextures(s,fs::absolute("tmp/export-media")/staging.stem(),media);
                        report.findings.insert(report.findings.end(),media.findings.begin(),media.findings.end());
                        report.operations.extend(media.operations);
                    }else {packageTextures(s,o.output,report);rebaseTextures(s,o.output);}
                    // Media failures are not waived by --allow-check-errors.
                    if(media.ok()) {
                        s.save(staging,false,o.embed);
                        auto extracted=cacheFor(staging);
                        Scene reopened;reopened.load(staging,o.embed?extracted:fs::path());Report check;
                        auto reopenedBones=resolve(reopened,check,o.ns,true);validate(reopened,reopenedBones,check,o.tolerance);
                        Report shaderCheck;
                        if(o.flat) {
                            validateFlatShaders(reopened,shaderCheck);
                            check.findings.insert(check.findings.end(),shaderCheck.findings.begin(),shaderCheck.findings.end());
                        }
                        Report mediaCheck;if(o.embed) {
                            validateEmbeddedTextures(reopened,extracted,mediaCheck,&embedded);
                            check.findings.insert(check.findings.end(),mediaCheck.findings.begin(),mediaCheck.findings.end());
                            check.operations.extend(mediaCheck.operations);
                        }
                        if(!shaderCheck.ok()||!mediaCheck.ok()||(!check.ok()&&!o.allowErrors)) {
                            report.add("error","export.roundtrip","保存後の再読み込みチェックに失敗しました。FBX は出力しません。",check.json());
                            fs::remove(staging);staging.clear();
                        } else {
                            fs::rename(staging,o.output);staging.clear();wrote=true;
                            report.add("info","export.roundtrip","FBX を保存し再読み込みしました。",check.json());
                        }
                    }
                }
            }
        }
        auto result=report.json();result["command"]=o.command;result["input"]=fs::absolute(o.input).string();
        result["output_written"]=wrote; if(wrote)result["output"]=fs::absolute(o.output).string();
        if(o.command=="configure")result["texture_storage"]=o.embed?"embedded":"external";
        result["scene"]=s.inventory();result["pose_angles"]=poseAngles(bones);
        writeReport(o.report,result);std::cout<<result.dump()<<'\n';
        return report.ok()?0:2;
    }catch(const std::exception& ex){
        if(!staging.empty()){std::error_code ec;fs::remove(staging,ec);}
        Json error=Json::object();error["ok"]=false;error["error"]=ex.what();std::cout<<error.dump()<<'\n';
        std::cerr<<ex.what()<<'\n';return 1;
    }
}
