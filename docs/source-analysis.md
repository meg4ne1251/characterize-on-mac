# 提供資料の解析と実装の対応

解析対象は `tmp/VLLModelConfigure2-2025.2/` の Python スクリプト8ファイル（計784行）と、`tmp/キャラクタライズ記事/` の Notion 書き出しです。原本は変更せず、Git 対象外の `tmp/` に保持します。

## 処理の対応

| 元スクリプト | 元の処理 | macOS 実装 |
|---|---|---|
| `Map_CharacterBone.py` | 73スロットとサークルのボーン名 | `mapping.inc`。MotionBuilder の名称と FBX SDK の enum が異なるため、73件すべて SDK のスロット名で検証 |
| `RenameSkeleton.py` | 既存 Character のスロットをもとに改名 | `--rename-from-character`。名前衝突を事前確認し、一時名を挟んで一括改名 |
| `Characterize.py` | 名前検査、マッピング、SetCharacterizeOn、可視化、ノードサイズ、X-Ray | 名前検査、15必須スロットと Parent の検査、階層検査、FbxCharacter とスタンスオフセット、Characterize フラグ、可視化、サイズ設定。X-Ray は Viewer 固有 |
| `ApplyTextures.py` | 既存メディア等の削除、画像名＝マテリアル名で Diffuse 適用 | `--textures`。同名候補の曖昧さ・欠落を検査。既存の他チャンネルやシェーダーは保持。処理済み FBX の画像を参照する `--texture-reference` も追加 |
| `GroupCreation.py` | Character のリンクとスキンメッシュを2グループへ | 同名の `FbxSelectionSet` を作成。対象外グループは保持。MotionBuilder の FBGroup としての表示は未確認 |
| `CheckConfiguration.py` | Character 数、名前、Reference、可視性、シェイプ重複、母音、テクスチャ、グループ | ファイル内の実データから検査。UV・画像の存在・グループのメンバー・Tポーズも検査。全件まとめて JSON 出力 |
| `ConfigureManager.py` | GUI 内の作業済みフラグ | フラグに依存せず、保存後も再検査できる形へ置き換え |
| `VLLModelConfigure2.py` | 5ボタンと Reset の GUI | 独立した CLI。入力を変更せず別ファイルへ出力するため、やり直しは入力から再実行 |

## 元スクリプトから修正した検査

- `IsShapeinMultiMesh` は検査用リストにシェイプ名を追加しておらず、重複を検出できません。新実装では、同じ Character にスキン接続した異なるメッシュをまたぐ同名チャネルを検出します。
- グループ未作成の警告後にも「正常完了」へ進む経路がありました。新実装はエラーを残し、不合格として返します。
- テクスチャやグループの「ある／作業ボタンを押した」だけでなく、使用中マテリアルの Diffuse 画像と対象グループの内容を検査します。
- 母音は `a, i, u, o` を必須、`e` を任意とする元の方針を維持します。
- `Character`、`character`、`chara` を含む名前、空白のみ、`1` を拒否します。ファイル操作を妨げるパス区切り等も拒否します。

## 資料・サンプルから分かったこと

- 上腕の現行名は `UpArm_L/R`。一部旧データでは `Arm_L/R`。
- 手の甲内の現行名は `InHandIndex_L` など。一部のサンプルでは `HandIndex_L.0`。指の関節にはドットなしの旧命名もあります。
- キャラクタ定義と HIK の実行は別です。FBX SDK に MotionBuilder の `FBCharacter.SetCharacterizeOn()` をそのまま呼ぶ API はありません。
- 提供された処理済みファイルは、SDK から読むと Character のグローバルスタンスオフセットと Characterize フラグを持ち、独立した CharacterPose は0件でした。新実装もリンクにオフセットを記録します。
- `_MB` でも腕・脚に1〜8度程度のずれが残っていました。自動補正ではそれより厳しい水平・垂直を目標にします。
- `Rakuteni_MB.fbx` には対象と別に `test_motion` Character が存在しました。ファイル全体を無条件の正解とは扱いません。
- 元記事では HIK Template を忘れず出力するよう案内していますが、元スクリプト自体にその出力処理はありません。この実装も助言として表示し、未確認の XML を互換テンプレートとして出力しません。

## 互換性の境界

SDK で Character 定義を書き、フラグとマッピングが保存後も残ることは確認しています。MotionBuilder がその定義をロック済み Character として正しく評価すること、HIK でリターゲットできること、SelectionSet が FBGroup として表示されることは、MotionBuilder 側での確認が必要です。テスト結果の `ok` は SDK 上の検査合格を表し、完全互換の証明ではありません。

これは実装の都合だけではなく、[Autodesk の公式説明](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder/files/GUID-96A9C00C-6423-4C7C-A6B6-0837F8196949.htm)でも、SDK 経由の FBX と MotionBuilder 固有情報を含むファイルを区別しているためです。

参照した API は [FbxCharacter](https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/class_fbx_character.html) と [FbxCharacterLink](https://help.autodesk.com/cloudhelp/2020/ENU/FBX-API-Reference/cpp_ref/class_fbx_character_link.html)、およびローカルの SDK 2020.3.11 のヘッダー・サンプルです。
