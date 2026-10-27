# characterize-on-mac

MotionBuilder で行っているキャラクタライズ作業を、macOS 上で実行できるようにするプロジェクトです。

## 基本方針

- 動作対象は macOS。
- FBX ファイルの処理には Autodesk FBX SDK を使う。
- Maya のインストールやライセンスを実行条件にしない。
- MotionBuilder の既存スクリプトと関連資料をもとに、必要な処理と出力を整理する。

現時点ではプロジェクトの初期整理のみを行っています。キャラクタライズ機能、SDK の導入、実行コマンドは未実装です。実装言語や UI の有無は今後決定します。

## 構成

```text
docs/
  requirements.md           要件・未確定事項・進め方
  references/              関連資料・リンク・作業手順
scripts/
  motionbuilder/           MotionBuilder で使っている既存スクリプト
src/                       macOS 向けの実装
tests/                     検証コード
data/                      ローカルの入力・出力データ（README 以外は Git 対象外）
tmp/                       一時ファイル（Git 対象外）
```

## 資料・スクリプトの追加

1. 関連資料や手順は [`docs/references/`](docs/references/README.md) に追加します。URL や文章だけでも構いません。
2. MotionBuilder の既存スクリプトは [`scripts/motionbuilder/`](scripts/motionbuilder/README.md) に追加します。
3. 検証用の FBX などは `data/`、一時作業用のファイルは `tmp/` に置きます。

FBX SDK 本体はリポジトリへ同梱せず、今後、導入手順と参照方法を文書化します。SDK をプロジェクト内に配置する場合は Git 対象外の `sdk/` または `.local/` を使います。

## GitHub

管理先: [meg4ne1251/characterize-on-mac](https://github.com/meg4ne1251/characterize-on-mac)（private）

一時ファイル、キャッシュ、ビルド生成物、ローカル設定、検証データは [`.gitignore`](.gitignore) で除外します。

今後の要件と検証項目は [`docs/requirements.md`](docs/requirements.md) にまとめます。
