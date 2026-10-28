# characterize-on-mac

サークルの MotionBuilder スクリプト **VLLModelConfigure2 2025.2** が行うモデル規格化を、macOS 上で実行するためのツールです。Autodesk FBX C++ SDK を使用し、Maya のインストール・ライセンスに依存しません。

現在は独立した CLI として動作します。提供された5つの処理前モデルで、Aポーズからの補正・規格化・保存後の SDK 検査を確認しています。MotionBuilder の HIK ソルバ・FBGroup の表示まで含めた完全互換は、まだ確認できていません。

## 実装済み

- サークルの73スロットに対応したボーンマッピング、15必須スロットと Parent・階層の検査。
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

プロジェクトのルートで実行します。

```sh
python3 scripts/setup_sdk.py
python3 scripts/build.py
mkdir -p data/output

build/characterize configure data/input/Gakupo.fbx \
  --output data/output/Gakupo_mac.fbx \
  --name Gakupo \
  --texture-reference data/expected/Gakupo_MB.fbx \
  --correct-tpose \
  --report tmp/Gakupo-result.json
```

既存の出力やレポートを置換するときは `--force` を追加します。通常の利用では `--texture-reference` の代わりに `--textures 画像フォルダ` を指定できます。入力の画像接続をそのまま使える場合は両方を省略できます。

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
- [元資料の解析と機能の対応、互換性の境界](docs/source-analysis.md)
- [実モデルによる検証結果](docs/validation.md)
- [Windows / MotionBuilder のテクスチャ検証結果](docs/windows-motionbuilder-results.md)
- [FBX の初期表示設定と Viewer の確認手順](docs/motionbuilder-display.md)
- [要件と今後の確認項目](docs/requirements.md)

## 構成

```text
src/characterize/           FBX SDK を使う C++ 実装
scripts/                   SDK の準備・ビルド・実モデル検証
scripts/motionbuilder/     MotionBuilder 側の参照・検証用
tests/                    ネイティブ検証、CLI 検証、データ保持の照合
docs/                     手順・解析・検証結果
data/input/               処理前モデル
data/expected/            サークルで処理した参照モデル
data/output/              このツールの出力・画像・レポート
tmp/                      提供資料の原本・SDK ダウンロード・一時ファイル
.local/                   ローカルの FBX SDK
build/                    実行ファイル・生成したテストデータ
```

`data/` は README 以外、`tmp/`、`.local/`、`build/` は全体を Git 対象外にしています。資料の追加場所は [docs/references/](docs/references/README.md) です。

管理先は [meg4ne1251/characterize-on-mac](https://github.com/meg4ne1251/characterize-on-mac)（private）です。
