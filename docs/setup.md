# セットアップと実行

## 必要な環境

- macOS と Xcode Command Line Tools（`xcrun clang++`）。
- Python 3.10 以降。Python はビルド・検証の起動に使います。
- Autodesk FBX C++ SDK 2020.3.11。

この Mac では macOS 27.0.1 / arm64 / Clang / Python 3.15.0 で動作確認しました。Maya、MotionBuilder、Python 用 FBX バインディング、CMake は実行に必要ありません。Intel Mac は未検証です。

## SDK とビルド

プロジェクトのルートで実行します。

```sh
python3 scripts/setup_sdk.py
python3 scripts/build.py
build/characterize --help
```

`setup_sdk.py` は [Autodesk 公式 SDK](https://aps.autodesk.com/developer/overview/fbx-sdk) を取得し、配布ファイルの SHA-256 を確認して `.local/fbxsdk/2020.3.11/` に展開します。macOS インストーラ本体や同梱スクリプトは実行しません。SDK の利用条件は同梱の `License.rtf` を参照してください。SDK 本体は Git には含めません。

既に独立した FBX SDK を導入している場合は、そのパスを指定できます。

```sh
export FBXSDK_ROOT='/Applications/Autodesk/FBX SDK/2020.3.11'
python3 scripts/build.py
```

## 内容の確認

```sh
build/characterize inspect data/input/Gakupo.fbx --report tmp/inspect.json
build/characterize check data/output/Gakupo_mac.fbx --report tmp/check.json
```

結果は標準出力にも JSON で出力します。`inspect` で Character の数・名前・スロット、ボーンの位置、シェイプキー、マテリアルと画像の対応を確認できます。フォルダ名や `_MB` の有無だけで処理済みと判定しません。

`check` は規格と Tポーズを検査します。角度の許容値は既定で1度です。MotionBuilder の警告基準を再現したものではありません。

埋め込み画像は `tmp/embedded/` のファイルごとのキャッシュに展開します。入力 FBX や、その隣のディレクトリへは書き込みません。キャッシュは削除して構いません。

## 規格化と Tポーズ補正

マテリアル名と同じ名前の画像をフォルダに用意した場合:

```sh
mkdir -p data/output
build/characterize configure data/input/Gakupo.fbx \
  --output data/output/Gakupo_mac.fbx \
  --name Gakupo \
  --textures data/textures/Gakupo \
  --correct-tpose \
  --report tmp/Gakupo-result.json
```

対応する処理済み FBX に正しい画像が入っている場合:

```sh
build/characterize configure data/input/Chis-A.fbx \
  --output data/output/Chis-A_mac.fbx \
  --name Chis-A \
  --texture-reference data/expected/Chis-A_MB.fbx \
  --correct-tpose \
  --report tmp/Chis-A-result.json
```

`--texture-reference` は同名マテリアルの Diffuse 画像を取得します。参照側に埋め込まれた画像も扱えます。ボーン・メッシュ・ウェイトは処理前 FBX のものを使います。`--textures` と同時には指定できません。

両方を省略した場合は入力側の画像接続を保持します。見つからない画像を別の画像で代用したり、複数の同名画像から適当に選択したりはしません。使用中の画像は出力隣の `<出力名>.assets/` にコピーし、絶対パスと相対パスを設定します。配布・移動時は FBX と `.assets` フォルダをセットにしてください。

既存 Character の対応付けを使ってボーンを改名し、作り直す場合:

```sh
build/characterize configure data/expected/Megane_TETO_MB.fbx \
  --output data/output/Megane_TETO_mac.fbx \
  --name Megane_TETO \
  --rename-from-character --replace-character --correct-tpose
```

`--rename-from-character` は Character が1つの場合に使えます。通常の処理でも、既知の旧命名（`Arm_L`、`HandIndex_L1`、`HandIndex_L.0` など）は現行名へ正規化します。名前空間は保持します。複数の同名ボーンがある場合は `--namespace 名前空間` を指定します。Spine の番号ずれや不明な命名は推測しません。

既存の出力・レポートへ再出力するときは `--force` を追加します。入力 FBX 自体への上書きは禁止しています。

## Tポーズ補正の範囲

腕の上下2区間を ±X、脚の上下2区間を −Y に整えます。親を先に回し、ワールド変換から FBX SDK でローカル回転を求めるため、pre/post rotation を含むスケルトンにも対応します。元の頂点、シェイプターゲット、ウェイト、バインド行列を維持し、スキンの変形でポーズを変えます。

- 最大補正角度は75度。`--max-correction-angle` で変更できます。
- アニメーションのキー、Character 以外の拘束、頂点キャッシュ、負のスケール、実質的な非一様スケールは停止します。
- Blender 由来の微小なスケール誤差は許容します（相対 `1e-4`）。
- 胴体が大きく傾いている場合や、+Z 前向きと左右の位置関係が一致しない場合は停止します。
- ボーンが短すぎる・階層が不正・補正後の角度が収束しない場合も停止します。
- 指の開き・手首のねじれ・肩の形・服や髪の貫通・足裏の接地まで自動修正する機能ではありません。

数値が合っていても、モデルごとの見た目は確認してください。

## 終了コードと保存

- `0`: このツールの検査に合格。
- `2`: 規格・ポーズの検査に不合格。
- `1`: 引数、ファイル、SDK などの実行エラー。

通常は検査不合格時に FBX を保存しません。`--allow-check-errors` を指定すると、定義作成後の規格チェックが不合格でも検証用 FBX を保存できます。終了コードは2のままで、`ok` は false です。マッピングやポーズ補正そのものの失敗は、このオプションでも続行しません。

FBX は一時ファイルへ書き出し、SDK で開き直して検査してから出力先へ移動します。途中で失敗した場合、コピー済みの `.assets` が残ることがあります。`motionbuilder_verified` は、この Mac での検査だけでは true にしません。

## テスト

```sh
python3 scripts/build.py --tests
build/native-tests
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 scripts/validate_models.py --force
```

最後のコマンドは `data/input/*.fbx` と、存在すれば `data/expected/*_MB.fbx` を使います。出力先は `data/output/`、詳細レポートは `data/output/reports/` です。入力ファイルの SHA-256 が処理前後で一致することも確認します。
