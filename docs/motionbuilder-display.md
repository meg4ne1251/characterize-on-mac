# FBX の初期表示設定

## 今回の変更

`configure` は標準で、全メッシュの表示指定を `FbxNode::eFullShading` に設定します。スキンの有無や `--flat-shader` の有無には依存しません。`inspect` / `check` は表示指定を変更しません。

Autodesk の [FbxNode API](https://help.autodesk.com/cloudhelp/2020/ENU/FBX-API-Reference/cpp_ref/class_fbx_node.html) では、FullShading はテクスチャとシーンの照明を使うモデル表示指定で、FBX に保存されます。提供された `data/expected/Chis-A_MB.fbx` と `Gakupo_MB.fbx` の全メッシュでも、この指定（ASCII では `Shading: U`）を確認しました。一方、以前の `Chis-A_mobu_fixed.fbx` には同じ表示指定がありませんでした。

この指定を出力し、保存後の再読み込みで全メッシュに保持されていることを検査します。保持されなければ `display.not_full_shading` として出力を停止します。この失敗は `--allow-check-errors` でも続行しません。

JSON の `operations` に `mesh_display`、`scene.meshes[]` に `shading_mode`（FBX SDK の enum 値）と `full_shading` を記録します。CLI 自身は MotionBuilder を実行しないため、新規出力の `viewer_display_verified` は false を維持します。個別ファイルに対するユーザーの実機確認は下記に記録します。

## Viewer 全体の設定との違い

MotionBuilder の [FBViewingOptions.DisplayMode](https://help.autodesk.com/cloudhelp/2026/ENU/MOBU-PYTHON-API-REF/classpyfbsdk_1_1_f_b_viewing_options.html) は、Viewer 全体の設定です。元の `ApplyTextures.py` は、MotionBuilder 実行中にこれを `kFBModelShadingAll` に切り替えています。

FBX のモデル表示指定が保存されることと、Viewer の Display メニュー自体が読み込み時に Textures + Shaders へ切り替わることは、別の検証事項です。Mac の FBX SDK だけでは後者を確認できませんが、今回の出力はユーザーの実機確認で初期状態のテクスチャ・シェーダー表示に合格しました。Viewer の未確認のプロパティや、別用途の `CameraViewingMode` / `DefaultViewingMode` を推測して追加していません。

ユーザーの希望に従い FBX 単体を優先し、起動スクリプトは追加していません。今回のユーザー環境では、起動スクリプトなしで希望する初期表示になりました。他モデル・他バージョンや、Viewer 設定による上書きの全組合せは未検証です。

## 実機確認済みの出力

```sh
build/characterize configure data/input/Chis-A.fbx \
  --output data/output/texture-check/Chis-A_textured_display.fbx \
  --name Chis-A --textures data/input/tex --reset-textures \
  --correct-tpose --flat-shader

build/characterize check data/output/texture-check/Chis-A_textured_display.fbx \
  --require-embedded-textures --require-flat-shader
```

以前の表示確認済みファイルを保持するため、別名で生成します。既存出力を置換するときだけ `--force` を指定します。

生成済みファイルの SHA-256 は `afe57247dd2e3175c1ce98d257439a26f36451c28549d4a2f455f97470d361bb` です。全4メッシュの FullShading は参照 FBX と一致し、全27画像の名前・内容・UV・動画接続、Flat 設定、入力とのデータ保持監査に合格しています。入力 FBX と提供画像の SHA-256 は変更なしです。詳細は `data/output/texture-check/reports/textured-display-summary.json` に保存しています。

自動テストはネイティブ **248 assertions**、CLI **22件**に合格しました。バイナリ・ASCII の保存後に FullShading が保持されることと、モデル・マテリアルを変更しないことを検証しています。

2026-10-28、ユーザーから `Chis-A_textured_display.fbx` を MotionBuilder で開き、初期状態でテクスチャとシェーダーが反映された表示になることを確認した、との報告を受領しました。FBX 単体で希望する初期表示になったことを、このファイルについて確認済みとします。メニュー項目そのものの選択状態や、全 Viewer 設定の組合せを網羅した検証を報告するものではありません。

再確認する場合は、Viewer を Shaders のみにしてから新しいシーンへ File > Open し、6キーや Display メニューを操作する前の表示を確認します。
