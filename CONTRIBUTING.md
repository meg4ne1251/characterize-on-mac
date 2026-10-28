# 開発と検証

ビルドには macOS、Xcode Command Line Tools、Python 3.10 以降、Autodesk FBX C++ SDK 2020.3.11 を使います。詳細は [セットアップ](docs/setup.md) を参照してください。

## ビルドと自動テスト

プロジェクトのルートで実行します。

```sh
python3 scripts/setup_sdk.py
python3 scripts/build.py
python3 scripts/build.py --tests
build/native-tests
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

自動テストは FBX SDK で合成フィクスチャを生成します。配布されていない実モデル・画像は不要です。生成物は `build/` と `tmp/` に置かれます。

実モデルを使う追加検証は任意です。自身で用意した入力 FBX を `data/input/`、対応する参照 FBX がある場合は `data/expected/` に置いて実行します。

```sh
python3 scripts/validate_models.py
```

既存出力を置換する場合は `--force` を付けます。モデルごとの入力条件と結果は [検証記録](docs/validation.md) にまとめています。

## ファイルの追加

- 実装は `src/characterize/`、テストは `tests/`、手順と検証記録は `docs/` に置きます。
- モデル・画像・参照データは `data/`、SDK は `.local/`、一時資料とログは `tmp/` に置きます。これらは Git 管理外です（`data/README.md` を除く）。
- FBX と `.fbm` 展開先は配置場所によらず Git 対象外です。合成フィクスチャはバイナリを追加せず、テスト内で生成します。
- 公開する説明やコードには、個人の絶対パス、認証情報、非公開の作業資料を含めないでください。

変更前に `git status --short`、提出前に `git diff --check` と `git diff` を確認してください。入力保護・保存・画像処理・ポーズ補正を変更した場合は、上記の自動テストを実行し、SDK 上の検査結果と MotionBuilder 実機の結果を区別して記録します。
