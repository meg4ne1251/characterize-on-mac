# 実装の設計と互換性

macOS で FBX モデルを規格化するための設計と、MotionBuilder 側で確認が必要な範囲をまとめます。モデル・画像・ローカルの検証資料はリポジトリに同梱していません。

## 処理と実装

| 処理 | 実装・確認内容 |
|---|---|
| ボーンマッピング | `src/characterize/mapping.inc` に73スロットを定義。MotionBuilder のスロット名と FBX SDK の enum は異なるため、全件の対応をテスト |
| 命名の正規化 | 既知の旧名を変換。`--rename-from-character` は既存 Character の対応付けを使い、名前衝突を確認して一括改名 |
| キャラクタ定義 | 15必須スロットと Parent・階層を検査し、`FbxCharacter`、スタンスオフセット、Characterize フラグ、可視性、サイズを設定 |
| Tポーズ補正 | 腕を水平、脚を垂直に補正。頂点・シェイプ・ウェイト・バインド行列を保持し、未対応の変換やアニメーションは停止 |
| 画像適用 | マテリアルと同名の画像、または参照 FBX の Diffuse 画像を適用。曖昧さ・欠落・UV を検査。標準で埋め込み、保存後に全画像を復元・照合 |
| グループ | Skeleton / Mesh の `FbxSelectionSet` を作成。対象外のグループは保持 |
| モデル検査 | Character 数、名前、Reference、可視性、シェイプ、母音、画像、UV、グループ、Tポーズを検査し JSON へ出力 |
| 保存 | 入力とは別のファイルへ出力。保存後に SDK で再読み込みして検査 |

## 命名と検査の方針

- 上腕は `UpArm_L/R`。旧名 `Arm_L/R` は正規化します。
- 手の甲は `InHandIndex_L` など。旧名 `HandIndex_L.0` や、指のドットなしの旧命名も正規化します。
- 同じ Character にスキン接続した異なるメッシュをまたぐ、同名シェイプチャネルを検出します。
- 母音は `a, i, u, o` を必須、`e` を任意とします。
- `Character`、`character`、`chara` を含む名前、空白のみ、`1`、パス区切りを含む名前を拒否します。
- グループや画像は存在だけでなく、対象メンバーや使用中マテリアルとの接続も検査します。

## 参照 FBX と検証

ローカルで使用した MotionBuilder 処理済み FBX には、Character のグローバルスタンスオフセットと Characterize フラグがあり、独立した CharacterPose は0件でした。本実装もリンクにオフセットを記録します。

参照ファイルにも腕・脚に1〜8度程度のずれがあり、`Rakuteni_MB.fbx` には対象と別の `test_motion` Character が存在しました。ファイル全体の一致を正解とせず、スロット・画像・形状データなどの項目ごとに検査します。最新の実モデルの結果と過去の記録は [検証結果](validation.md) を参照してください。

## 互換性の境界

キャラクタ定義と HIK の実行は別です。FBX SDK に MotionBuilder の `FBCharacter.SetCharacterizeOn()` をそのまま呼ぶ API はありません。SDK で定義・マッピング・フラグが保存後も残ることは検査しますが、MotionBuilder のロック済み Character としての評価、HIK のリターゲット、SelectionSet の FBGroup としての表示は実機確認が必要です。テスト結果の `ok` は SDK 上の検査合格を表します。HIK Template の出力は実装していません。

`--flat-shader` は、参照 FBX の `MotionBuilder_Generic` / `FlatShader` をもとに全メッシュへシェーダー接続と設定を作成します。マテリアルの `ShadingModel` は変更しません。SDK での保存後検査に加え、Chis-A の修正候補について Windows / MotionBuilder 2026 で模様の表示と FBX 単体の移動を確認しています。他モデルや透明部分の厳密な描画は未検証です。[Windows の検証結果](windows-motionbuilder-results.md) と [Flat シェーダーの適用](setup.md#flat-シェーダーの適用) を参照してください。

`configure` は全メッシュへ FullShading 表示指定を保存します。Chis-A では MotionBuilder で開いた初期状態の表示も確認しています。[初期表示の検証結果](motionbuilder-display.md) を参照してください。CLI は個々の出力の実機検証を行わないため、`motionbuilder_verified` は false のままです。

## API の参照先

- [Autodesk: FBX ファイルと MotionBuilder 固有情報](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder/files/GUID-96A9C00C-6423-4C7C-A6B6-0837F8196949.htm)
- [FbxCharacter](https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/class_fbx_character.html)
- [FbxCharacterLink](https://help.autodesk.com/cloudhelp/2020/ENU/FBX-API-Reference/cpp_ref/class_fbx_character_link.html)
- ローカルの FBX SDK 2020.3.11 に同梱されたヘッダー・サンプル。
