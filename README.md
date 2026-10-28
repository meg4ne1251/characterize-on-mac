# characterize-on-mac

FBX モデルのキャラクタ定義・ボーン名・テクスチャを整え、AポーズからTポーズへの補正を macOS 上で実行する CLI ツールです。Autodesk FBX C++ SDK を使用し、Maya のインストール・ライセンスに依存しません。

macOS / Apple Silicon で動作確認しています。実モデル5件で補正・保存後の SDK 検査を行い、入力条件による出力停止も確認しています。最新の結果は [検証記録](docs/validation.md) を参照してください。MotionBuilder の HIK ソルバ・FBGroup の表示まで含めた完全互換は、まだ確認できていません。

## 実装済み

- 73スロットに対応したボーンマッピング、15必須スロットと Parent・階層の検査。
- 既存 Character を使う改名、旧命名の正規化、名前衝突の検出。
- FBX Character 定義、スタンスオフセット、Characterize フラグ、可視性とサイズの設定。
- Aポーズやわずかな傾きから、腕を水平・脚を垂直にする補正。
- マテリアル名と同名の画像による Diffuse 適用、処理済み FBX からの画像参照。
- 画像を標準で FBX に埋め込み、FBX 単体から全画像を復元・照合。明示指定で外部画像の同梱も可能。
- Skeleton / Mesh の SelectionSet 作成。
- `configure` で全メッシュの表示指定を FullShading に設定。FBX 保存後の保持を検査し、Chis-A は MotionBuilder で開いた直後のテクスチャ・シェーダー表示をユーザー確認済み。
- 全メッシュへの MotionBuilder Flat シェーダーの設定（`--flat-shader`）、保存後の設定・接続の検査。Chis-A の修正候補は Windows / MotionBuilder 2026 で模様の表示と FBX 単体の移動を確認済み。
- 母音シェイプ、メッシュ間のシェイプ重複、画像、UV、グループなどの検査と助言。
- JSON レポート、保存後の再読み込み検査、元データを変更しない別ファイルへの出力。

## 実行

macOS、Xcode Command Line Tools、Python 3.10 以降が必要です。プロジェクトのルートで実行します。

```sh
python3 scripts/setup_sdk.py
python3 scripts/build.py
mkdir -p data/output

build/characterize configure data/input/Model.fbx \
  --output data/output/Model_mac.fbx \
  --name Model \
  --textures data/textures/Model \
  --correct-tpose \
  --report tmp/Model-result.json
```

モデル・画像・検証用の参照 FBX は同梱していません。自身で用意した FBX を `data/input/`、マテリアル名と同じ名前の画像を `data/textures/Model/` に置いてください。対応するボーン名は [マッピング定義](src/characterize/mapping.inc) を参照してください。入力の画像接続をそのまま使える場合は `--textures` を省略できます。処理済み FBX の画像を使う場合は、代わりに `--texture-reference 参照.fbx` を指定できます。

既存の出力やレポートを置換するときは `--force` を追加します。入力 FBX と参照 FBX は上書きしません。

Chis-A の単色表示を修正した、Windows で表示確認済みの設定を使う場合:

```sh
build/characterize configure data/input/Chis-A.fbx \
  --output data/output/Chis-A_mobu_fixed.fbx --name Chis-A \
  --textures data/input/tex --reset-textures --correct-tpose --flat-shader
```

`--reset-textures` は既存の透明度・法線などの画像接続も削除して画像を適用し直すため、明示指定にしています。画像から選ぶ UV 名、動画接続、元の画像名を保った埋め込みは修正済みです。確認範囲と原因分析は [Windows の検証結果](docs/windows-motionbuilder-results.md) を参照してください。

画像は標準で FBX に埋め込まれるため、出力 FBX だけを渡せます。外部画像で運用する場合だけ `--external-textures` を付け、出力隣の `.assets` フォルダも同梱してください。以前作成した外部参照版の FBX は、再出力するまで埋め込みには変わりません。

全メッシュを Flat シェーダーにする場合は `configure` に `--flat-shader` を追加します。既存のシェーダー接続を置き換え、マテリアルの色・画像・透明度は保持します。省略した場合はシェーダーを変更しません。

- [セットアップ・全オプション・テスト方法](docs/setup.md)
- [実装の設計と互換性の境界](docs/source-analysis.md)
- [実モデルによる検証結果](docs/validation.md)
- [Windows / MotionBuilder のテクスチャ検証結果](docs/windows-motionbuilder-results.md)
- [FBX の初期表示設定と Viewer の確認手順](docs/motionbuilder-display.md)
- [要件と今後の確認項目](docs/requirements.md)
- [開発・テスト・ファイルの追加方法](CONTRIBUTING.md)

## 構成

```text
src/characterize/           FBX SDK を使う C++ 実装
scripts/                   SDK の準備・ビルド・実モデル検証
scripts/motionbuilder/     MotionBuilder 側の検証スクリプトの追加先
tests/                    ネイティブ検証、CLI 検証、データ保持の照合
docs/                     手順・解析・検証結果
data/input/               処理前モデル
data/expected/            MotionBuilder で処理した比較用モデル
data/output/              このツールの出力・画像・レポート
tmp/                      SDK ダウンロード・一時ファイル・ローカル資料
.local/                   ローカルの FBX SDK
build/                    実行ファイル・生成したテストデータ
```

`data/` は README 以外、`tmp/`、`.local/`、`build/` は全体を Git 対象外にしています。資料の追加場所は [docs/references/](docs/references/README.md) です。

SDK 本体は同梱していません。利用条件と導入手順は [セットアップ](docs/setup.md#sdk-とビルド) を参照してください。
