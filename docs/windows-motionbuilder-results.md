# Windows / MotionBuilder テクスチャ検証結果（2026-10-28）

## 結果と根拠

`data/ascii-test/test-results.md`、スクリーンショット、`diagnostics.json`、画像照合結果を解析しました。Windows / MotionBuilder 2026（API version 26000.0）で、旧版の単色表示を再現し、修正候補では顔・髪・服・スカート・靴の模様が表示されています。どちらも ASCII パスの新しいシーンへ File > Open し、Textures + Shaders で比較した結果です。

| 項目 | 旧版 | 修正候補 |
| --- | --- | --- |
| 模様の表示 | 不合格 | 合格（確認した正面表示） |
| Diffuse の画像・Video・非ゼロ寸法 | 27/27 正常 | 27/27 正常 |
| UVSet | `default` | `UVMap` |
| UseMaterial | true | false |
| テクスチャ / VideoClips | 81 / 54 | 27 / 27 |
| 展開画像 | 12 | 27（名前・SHA-256 が原画像と一致） |
| FBX 単体の別フォルダへの移動 | 未実施 | 表示成功、展開画像 27/27 一致 |

全4メッシュで `Characterize_Flat` の Enable、ColorSource、Alpha、Transparency の復元も確認されています。

試験ファイルの SHA-256 を Mac 側の前回 manifest と再照合し、旧版・候補・移動後候補が想定したファイルであることを確認しました。候補と移動後候補の展開画像も、Mac にある提供 PNG と全27枚が一致しました。

- 旧版: `84eeff0c4e7268ad2a6c59e40dfb7cfa585527733a2a45a670ff3422425131b3`
- 表示確認済み候補: `5331032138427f4a416555f43362cabbd9cfe3f40b8a1a40b7b4c37fd6be2bd0`

## 原因の評価とコードへの反映

旧版でも全 Diffuse 画像のロードは正常でした。この試験から、画像欠落・展開失敗・ゼロ寸法を主因とは判断できません。旧版の `UVSet=default` は、実際のメッシュの UV 名 `UVMap` / `UV1` と一致しません。現行のチェックでは、この旧版から `check.texture_uv` を検出できます。これは具体的な設定不整合ですが、今回の試験は複数の変更を同時に適用した比較なので、単独原因とまでは断定しません。ハッシュ化した画像名や日本語パスを原因とする証拠もありません。

前回の作業ツリーに残っていた修正候補を維持し、次の動作を回帰検証しました。

- フォルダからの画像適用は、実在する先頭 UV 名を選び、`UseMaterial=false` とする。共有マテリアルで先頭 UV 名が異なる場合は空文字で各メッシュの先頭 UV を使う。
- `FbxVideo` の通常の接続と `VideoProperty` の接続を両方作る。参照 FBX からは画像だけでなく UV・変換・ラップ等の設定もコピーする。
- `--reset-textures` で古い透明度等の画像接続を削除して再適用する。ほかの画像チャンネルを消すため、引き続き明示指定とする。
- 埋め込み時は元画像名を保持し、同名で内容が異なる場合のみ接尾辞で区別する。

今回、`inspect` / `check` に UseMaterial・マッピング種別・動画パス・VideoProperty 接続の情報を追加しました。また、外部画像出力でも明示的な Video のパスを `.assets` 内へ更新するよう修正し、Texture と Video が別の場所を指す状態を防ぎました。外部画像のハッシュ命名は従来どおりです。

## Mac での再生成と検証

```sh
python3 scripts/build.py
python3 scripts/build.py --tests
build/native-tests
python3 -m unittest discover -s tests -p 'test_*.py' -v

build/characterize configure data/input/Chis-A.fbx \
  --output data/output/texture-check/Chis-A_mobu_fixed.fbx \
  --name Chis-A --textures data/input/tex --reset-textures \
  --correct-tpose --flat-shader

build/characterize check data/output/texture-check/Chis-A_mobu_fixed.fbx \
  --require-embedded-textures --require-flat-shader

build/native-tests --audit data/input/Chis-A.fbx \
  data/output/texture-check/Chis-A_mobu_fixed.fbx
```

既存出力を置換する場合のみ `--force` を付けます。旧版・Windows 試験の原本は保持しています。

ネイティブテスト **240 assertions**、CLI 結合テスト **21件**に合格しました。共有マテリアルで先頭 UV 名が異なる場合、実際の UV 名が `default` の場合、保存後の UV・UseMaterial・動画接続、外部画像の Texture / Video パスの一致も確認しています。

検証結果は `data/output/texture-check/reports/mobu-fixed-summary.json` と各 `Chis-A-mobu-fixed*.json` に保存しています。新しく生成した FBX の全27 Diffuse の UV・UseMaterial・動画接続・画像名・画像内容と Flat 設定は、Windows で表示確認済みの候補と一致しました。入力とのデータ保持監査は、Windows 試験の候補自体と再生成版の両方に合格しました（頂点82,270、シェイプターゲット56、ウェイト161,094）。入力 FBX と提供画像の SHA-256 は処理前後で一致しています。旧版からは `check.texture_uv` を27件検出しました。

再生成版 `Chis-A_mobu_fixed.fbx` の SHA-256 は `f3e07976012473a9a6c9cf8b79971811ea2e79c6d9a6c05a2062749ac5604b48` です。

## 確認範囲

Windows の描画合格は上記ハッシュの候補とそのコピーに対する結果です。再生成した FBX は Mac 側で構造・画像・データ保持を検査しており、Windows で再び開いたという意味ではありません。CLI の `motionbuilder_verified` は、新規出力に実機検証を引き継がせないため false を維持します。

透明部分の全方向・重なり順・半透明、正常な参照 FBX との厳密な外観比較、変更を一つずつ戻す切り分け、HIK・リターゲット、他モデル・他バージョンの表示は未検証です。元の Windows 報告は変更していません。`data/` は Git 対象外のため、この文書に結果とファイルの識別情報を残しています。

## 初期表示設定の追加と実機確認

FullShading を全メッシュへ保存する変更後の `Chis-A_textured_display.fbx` について、2026-10-28 にユーザーから、MotionBuilder で開いた初期状態でテクスチャとシェーダーが反映されることを確認したとの報告を受領しました。起動スクリプトの追加は不要でした。対象ファイルの SHA-256 と詳細は [初期表示の検証結果](motionbuilder-display.md) に記録しています。この確認は上記の候補比較試験とは別の、今回の再生成ファイルに対する確認です。
