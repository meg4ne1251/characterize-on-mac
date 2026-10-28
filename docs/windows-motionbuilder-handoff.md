# Windows / MotionBuilder テクスチャ表示不具合の申し送り

> 2026-10-28 更新: Windows からの結果を受領しました。旧版の症状を再現し、下記の修正候補は MotionBuilder 2026 で模様の表示と FBX 単体の移動に合格しています。解析・実装・再生成の結果は [Windows の検証結果](windows-motionbuilder-results.md) を参照してください。以下は Windows テスト前の申し送りとして保持しています。

このプロジェクトの FBX 出力を、Windows 版 Autodesk MotionBuilder 上で実際に開き、テクスチャが表示されない原因を特定・修正してください。SDK 上で埋め込みが復元できることと、MotionBuilder で正しく描画されることを区別してください。以下の修正候補はまだ MotionBuilder で動作確認できていません。

## ユーザーが確認した症状

- 対象は `data/output/texture-check/Chis-A_embedded.fbx`。
- Windows の MotionBuilder で、一部が単色で塗られるだけで、テクスチャの模様が表示されない。
- Viewer を **Texture + Shader** に変更しても同じ症状。表示モードの切り替えだけでは解決しないことは確認済み。
- テクスチャ画像自体は FBX に埋め込まれている。
- 展開した画像名がランダムな文字列に見える問題もある。元画像には `Body.png`、`Face.png` など意味のある名前がある。
- MotionBuilder のバージョンと、どの部位が単色になるかの詳細は未確認。

## プロジェクトとデータ

- private リポジトリ: https://github.com/meg4ne1251/characterize-on-mac
- Mac 側の作業ディレクトリ: `/Users/yuta/dev/characterize-on-mac`
- C++ / Autodesk FBX SDK 2020.3.11 による独立 CLI。Maya や MotionBuilder を使わず、キャラクタライズ、A→T 補正、画像適用、Flat シェーダー、FBX 出力を行う。
- 入力: `data/input/Chis-A.fbx`
- 指定する画像フォルダ: `data/input/tex/`
- MotionBuilder で処理済みの比較用 FBX: `data/expected/Chis-A_MB.fbx`。この参照 FBX が今回の Windows 環境でも正常に表示されるか、最初に確認すること。
- 不具合報告対象: `data/output/texture-check/Chis-A_embedded.fbx`
- 今回の修正候補: `data/output/texture-check/Chis-A_mobu_candidate.fbx`
- 元の MotionBuilder スクリプト: `tmp/VLLModelConfigure2-2025.2/VLLModelConfigure2_modules/ApplyTextures.py`

**`data/`、`tmp/`、`build/`、`.local/` は Git 管理外です。GitHub から clone するだけでは上記のモデル・画像・元スクリプトは取得できません。Mac から対象ファイルをコピーしてください。Mac の `build/characterize` も Windows では実行できません。**

今回の C++ とテストの変更は、申し送り作成時点では Mac の作業ツリー内の未コミット変更です。GitHub に反映したとは扱わないでください。変更一式のパッチは `tmp/windows-handoff/current-worktree.patch` に出力しています。Windows の同じベースコミットに対し `git apply --check` で確認してから適用するか、最新の作業ツリーをコピーしてください。ベースコミットとファイルの SHA-256 は同フォルダの `manifest.json` に記録しています。

## これまでに確認できた事実

旧 `Chis-A_embedded.fbx` の Mac 側検証では、27 マテリアルの Diffuse に画像があり、全 4 メッシュに Flat が接続されている。既存の他チャンネルも残っているため、全 81 テクスチャ、内容の異なる画像 12 種が埋め込まれている。FBX だけを別フォルダへ移動し、新しい展開先へ復元した画像内容の照合は合格している。元の頂点・ポリゴン・UV・シェイプ・ウェイト・バインド行列の保持検査も合格していた。

ただし、この検査だけでは Windows / MotionBuilder での描画を保証できない。ユーザーの実機確認では旧版の表示は不合格である。`motionbuilder_verified` は false のまま維持すること。

画像名がハッシュになっていたのは、FBX の必須仕様ではなく本ツールの実装。`src/characterize/media.cpp` の `prepareEmbeddedTextures()` が、同名の別画像による上書きを避けるため、画像内容の FNV ハッシュとサイズから保存名を生成していた。旧外部画像出力にも `main.cpp` の `contentKey()` によるハッシュ名の処理がある。ハッシュ化そのものが表示不具合の原因と確定したわけではない。

## 参照 FBX との差分と、今回の修正候補

関連実装は `src/characterize/scene.cpp`、`media.cpp`、`main.cpp`、`shaders.cpp`。

1. **画像メディアとの接続形式**
   - 参照 FBX の各テクスチャには、画像 `FbxVideo` からテクスチャへの通常の OO 接続に加え、`VideoProperty` への OP 接続がある。
   - 旧 CLI が新規作成した Diffuse テクスチャには、SDK が自動生成する OO 接続だけがあり、`VideoProperty` の OP 接続がなかった。
   - 修正候補は `FbxVideo` を明示的に作成し、OO と `FbxReferenceDT` の `VideoProperty` への OP 接続を両方作る。
   - これは参照との確実な構造差だが、**この差分が今回の表示不具合の原因かは未確定**。実機で検証すること。
2. **UV とマテリアル利用設定**
   - 旧 CLI の新規画像は `UVSet="default"` の SDK 初期値。Chis-A の実際の UV 名は `UVMap` / `UV1`。参照 FBX は空文字で先頭 UV を使う設定。
   - 修正候補のフォルダからの画像適用は、使用先の先頭 UV 名が共通ならその名前を指定し、異なる場合は空文字で各メッシュの先頭 UV を使う。
   - 旧 CLI は `UseMaterial=true`。参照は false。新規画像を参照と同じ `eDefaultMaterial` に変更した。
   - 存在しない UV 名を指定した Diffuse は、新たに `check.texture_uv` で検出する。
   - これらも原因と断定せず、読み込み後の設定と実表示で検証すること。
3. **参照画像のコピー**
   - 旧 `applyReferenceTextures()` は画像パスだけを取り、新しいテクスチャを初期値で作り直していた。
   - 修正候補は名前・UV・位置・回転・拡縮・ラップ等の値を `Copy()` で引き継ぎ、画像と動画の接続は出力側 Scene に作り直す。別 SDK Manager のオブジェクトは共有しない。
4. **古い画像チャンネルの残存**
   - 旧 CLI は Diffuse だけを置換。入力の Blender 由来の `alpha_texture` が `TransparencyFactor` に残っていた。参照 FBX にはこの接続がない。
   - 元の MotionBuilder 用 `ApplyTextures.py` は、Scene の動画・テクスチャ・シェーダーを削除し、Diffuse を適用し直す。
   - 修正候補に明示オプション `--reset-textures` を追加した。出力側 Scene の全テクスチャと動画を削除してから、指定画像を再適用する。スカラーのマテリアル値とジオメトリは保持する。シェーダー置換は従来どおり `--flat-shader` が担当。
   - リセットは他チャンネルの画像も削除するため、通常の省略時は従来どおり保持する。候補 FBX はリセットを指定して作成する。
5. **意味のある画像名**
   - 埋め込み準備で元のファイル名を保持するよう変更した。内容が同じでも `Body.png` と `Underwear.png` はそれぞれの名前を保つ。
   - 同じファイル名で内容が異なる場合だけ、元の stem にハッシュ等を付けて区別する。
   - 今回変更した命名は埋め込み方式。`--external-textures` のハッシュ名は現時点で変更していない。

Flat シェーダーは、提供された参照 FBX の `MotionBuilder_Generic` / `FlatShader`、ColorSource、Alpha、Transparency、バージョン用 Blob 等を模した実装である。これについても SDK での保存後検査はあるが、MotionBuilder での正しい復元・表示は未保証。ここも原因候補から除外しないこと。

## Windows 側で進めてほしい検証

1. 実際の MotionBuilder バージョンを記録する。新しいシーンへ「Open」で旧 FBX を開いて再現する。既存シーンへの Merge で接続やシェーダーが混ざる状況を避ける。
2. Texture + Shader でも模様が出ないことを確認し、参照 `Chis-A_MB.fbx` と候補 `Chis-A_mobu_candidate.fbx` をそれぞれ新しいシーンで開いて比較する。旧版は上書きしない。
3. Navigator の Materials / Textures / Videos で、単色になる部位のマテリアル → Diffuse Texture → Video が実際に復元されているか調べる。Python なら `material.GetTexture(FBMaterialTextureType.kFBMaterialTextureDiffuse)`、その `texture.Video`、`video.Filename` / `Width` / `Height` を調べる。`Video` がない、Windows に存在しない Mac のパスを参照している、画像サイズが 0 等を区別する。
4. `.fbm` 展開先の画像が存在して内容を読み込めるか確認する。SDK で埋め込みが確認済みでも、MotionBuilder の展開・パス解決・画像ロードは別に確認する。
5. UV マッピング、UVSet、texture の変換、UseMaterial、モデル側の ShadingMode、Flat シェーダーの色の参照元・有効状態・接続を比較する。必要なら候補変更を一つずつ切り分け、何が必要だったか特定する。
6. Python で実在する画像を `FBTexture(path)` として新規作成し、対象マテリアルの Diffuse に接続した場合に表示されるかを試す。元スクリプトの `ApplyToMaterials()` も比較に使える。入力原本を上書きせず、作業用コピーで行う。
7. MotionBuilder 内で作った正常な設定を別 FBX に保存し、CLI 出力との構造差を比較する。根拠のないメタデータ追加を続けず、正常な実機出力を根拠に生成処理へ戻す。
8. 最終的に FBX 単体を別フォルダへコピーし、元画像・既存 `.fbm` キャッシュに頼らず再度開く。顔・髪・服の模様と透明部分を目視確認する。確認した範囲と残る問題を記録する。

必要に応じて MotionBuilder 内で読み取りだけを行う診断例:

```python
from pyfbsdk import FBSystem, FBMaterialTextureType
import os

for material in FBSystem().Scene.Materials:
    texture = material.GetTexture(FBMaterialTextureType.kFBMaterialTextureDiffuse)
    video = texture.Video if texture else None
    print("MATERIAL", material.LongName)
    print("  TEXTURE", texture.LongName if texture else None)
    print("  VIDEO", video.LongName if video else None)
    if video:
        filename = getattr(video, "Filename", "")
        print("  FILE", filename, "exists", os.path.isfile(filename))
        print("  SIZE", getattr(video, "Width", None), getattr(video, "Height", None))
```

この診断例は Mac では pyfbsdk がないため未実行。対象バージョンの API に合わせて調整すること。保存・削除・マテリアルの変更は行わない例である。

API の確認先: Autodesk の [Materials and Textures](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder-SDK/Scene-Elements/FBMaterial-and-FBTexture.html)、[FBVideoClip](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder-SDK/cpp_ref/class_f_b_video_clip.html)。

## 今回の Mac 側の検証結果

- 修正後のビルド成功。ネイティブテスト **236 assertions 合格**、CLI 結合テスト **20 件合格**。
- 候補 `Chis-A_mobu_candidate.fbx` の保存後検査、`check --require-embedded-textures --require-flat-shader`、入力とのモデルデータ保持監査に合格。
- 候補の全 27 マテリアルについて、復元した Diffuse 画像の名前が提供 PNG の名前と一致し、SHA-256 が一致することを確認。
- 候補の全 27 Diffuse で、保存後の `VideoProperty` に画像メディアが接続され、`UVSet=UVMap`、`UseMaterial=false`、古い `TransparencyFactor` の画像接続なしを確認。
- 詳細 JSON: `tmp/Chis-A-mobu-candidate.json`、`tmp/Chis-A-mobu-candidate-check.json`、`tmp/Chis-A-mobu-candidate-preservation.json`。
- **Windows / MotionBuilder での候補の描画は未検証。表示修正が完了したとは扱わないこと。**

## Mac 側で候補を再生成するコマンド

```sh
python3 scripts/build.py
python3 scripts/build.py --tests
build/native-tests
python3 -m unittest discover -s tests -p 'test_*.py' -v

build/characterize configure data/input/Chis-A.fbx \
  --output data/output/texture-check/Chis-A_mobu_candidate.fbx \
  --name Chis-A --textures data/input/tex --reset-textures \
  --correct-tpose --flat-shader \
  --report tmp/Chis-A-mobu-candidate.json

build/characterize check data/output/texture-check/Chis-A_mobu_candidate.fbx \
  --require-embedded-textures --require-flat-shader

build/native-tests --audit data/input/Chis-A.fbx \
  data/output/texture-check/Chis-A_mobu_candidate.fbx
```

候補出力・レポートがすでに存在する場合だけ `--force` を付ける。Windows で C++ をビルドする場合は SDK の Windows 版とビルド設定が別途必要。`scripts/build.py` は現在 macOS / xcrun 専用なので、そのまま Windows で実行できるとは扱わない。

## 完了条件

「FBX に画像が入っていた」「SDK の検査が通った」だけで完了としない。Windows / MotionBuilder で旧版の症状を再現し、修正版で正しい模様が表示されること、FBX 単体の移動後も表示されること、モデルの元データを保持していることを確認する。原因・修正箇所・MotionBuilder バージョン・検証結果をこのリポジトリに記録する。
