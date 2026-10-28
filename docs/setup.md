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

埋め込み画像は `tmp/embedded/` の読み込みごとに新しいキャッシュへ展開します。同じパスの FBX が差し替わった場合にも、以前の画像を流用しません。入力 FBX や、その隣のディレクトリへは書き込みません。キャッシュは削除して構いません。

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

両方を省略した場合は入力側の画像接続を保持します。見つからない画像を別の画像で代用したり、複数の同名画像から適当に選択したりはしません。

### 画像の埋め込み（標準）

画像は標準でバイナリ FBX に埋め込みます。`--embed-textures` で明示することもできます。渡すファイルは FBX だけでよく、`tex` や `.assets` フォルダを一緒に渡す必要はありません。読み込み側の MotionBuilder などが `.fbm` フォルダを作ることはありますが、FBX 内の画像を自動展開したもので、事前に用意するものではありません。

```sh
build/characterize configure data/input/Chis-A.fbx \
  --output data/output/Chis-A_mobu_fixed.fbx \
  --name Chis-A --textures data/input/tex --reset-textures \
  --correct-tpose --flat-shader

build/characterize check data/output/Chis-A_mobu_fixed.fbx \
  --require-embedded-textures --require-flat-shader
```

保存後は新しいキャッシュへ画像を展開し、外部パスへフォールバックせずに FBX 内から全テクスチャを復元できることと、保存前後の画像内容・接続数が一致することを検査します。埋め込み画像は元のファイル名を保ち、同名で内容が異なる場合だけ識別用の接尾辞を付けます。入力の画像名や内容は変更しません。

通常は Diffuse 以外の既存の画像接続も保持して埋め込みます。元の MotionBuilder スクリプトのように古い画像接続を消して適用し直す場合は、`--textures` または `--texture-reference` とあわせて `--reset-textures` を指定します。出力側 Scene の全テクスチャ・動画を削除するため、透明度・法線等の画像接続も削除されます。マテリアルのスカラー値は保持します。上のコマンドは、Chis-A の修正候補を Windows / MotionBuilder 2026 で表示確認したときの設定です。確認範囲は [Windows の検証結果](windows-motionbuilder-results.md) を参照してください。

フォルダから適用する画像には、使用先メッシュの先頭 UV 名を指定します。マテリアルを共有するメッシュで先頭 UV 名が異なる場合は、空文字で各メッシュの先頭 UV を選びます。存在しない UV 名は `check.texture_uv` エラーです。`inspect` / `check` の `scene.materials[].diffuse_textures[]` で、`uv_set`、`use_material`、`videos`（画像パスと `VideoProperty` 接続）を確認できます。

埋め込み準備の画像コピーは `tmp/export-media/`、再読み込み時の画像は `tmp/embedded/` に置きます。配布には不要なキャッシュで、削除できます。埋め込む画像が見つからない場合、画像シーケンスの場合、保存後に復元・照合できない場合は出力しません。この失敗は `--allow-check-errors` でも続行しません。

`check --require-embedded-textures` は毎回新しい展開先を使います。単に外部画像が存在するだけの FBX は、この検査に合格しません。`configure` の JSON の `texture_storage` は `embedded`、埋め込み情報は `operations` の `embed_textures`、再読み込み検査結果は `export.roundtrip` の詳細に記録します。

### 外部画像での運用

従来の外部参照版が必要な場合は `--external-textures` を付けます。出力隣の `<出力名>.assets/` に画像をコピーし、絶対パスと相対パスを設定します。この方式だけは、配布・移動時に FBX と `.assets` フォルダをセットにしてください。`--embed-textures` と同時には指定できません。既存の外部参照版も再度 `configure` で出力すれば埋め込み版にできます（既存 Character がある場合は `--replace-character` も必要）。

既存 Character の対応付けを使ってボーンを改名し、作り直す場合:

```sh
build/characterize configure data/expected/Megane_TETO_MB.fbx \
  --output data/output/Megane_TETO_mac.fbx \
  --name Megane_TETO \
  --rename-from-character --replace-character --correct-tpose
```

`--rename-from-character` は Character が1つの場合に使えます。通常の処理でも、既知の旧命名（`Arm_L`、`HandIndex_L1`、`HandIndex_L.0` など）は現行名へ正規化します。名前空間は保持します。複数の同名ボーンがある場合は `--namespace 名前空間` を指定します。Spine の番号ずれや不明な命名は推測しません。

既存の出力・レポートへ再出力するときは `--force` を追加します。入力 FBX と `--texture-reference` の参照 FBX への上書きは禁止しています。

## Flat シェーダーの適用

`configure` は `--flat-shader` の有無にかかわらず、全メッシュに FBX の `FullShading` 表示指定を保存します。提供された MotionBuilder 参照 FBX と同じ `Shading: U` です。保存後に再読み込みして保持を検査します。マテリアルやシェーダーの種類を変更する処理とは別です。Chis-A_textured_display.fbx は、MotionBuilder で開いた初期状態からテクスチャとシェーダーが反映されることをユーザー確認済みです。[確認結果と制限](motionbuilder-display.md)を参照してください。

`configure` に `--flat-shader` を付けると、全メッシュに MotionBuilder の Flat シェーダーを適用します。Character にスキン接続していないメッシュや、マテリアルのないメッシュも対象です。

```sh
build/characterize configure data/input/Gakupo.fbx \
  --output data/output/Gakupo_flat.fbx \
  --name Gakupo \
  --texture-reference data/expected/Gakupo_MB.fbx \
  --correct-tpose --flat-shader \
  --report tmp/Gakupo-flat-result.json

build/characterize check data/output/Gakupo_flat.fbx --require-flat-shader
```

MotionBuilder のシェーダーはマテリアルへの値ではなく、モデルへの接続として保存されます。このオプションは、共有する1つの Flat シェーダーで全メッシュの既存シェーダー接続を置き換えます。複数マテリアルを持つメッシュにも適用されます。以前のシェーダーオブジェクト自体と、メッシュ以外への接続は保持します。使われていないマテリアルには表示対象がありません。

Flat の色の参照元はマテリアル・テクスチャ、シェーダーの Alpha は1、Transparency は Accurate Transparency とし、元マテリアルの色・画像接続・透明度は変更しません。`ShadingModel` の文字列やポリゴン法線の変更ではありません。`--flat-shader` を省略すると既存シェーダーを保持します。再度適用しても、このツールが作成したシェーダーを再利用します。

実装は、提供された Gakupo / Chis-A / kiritan の処理済み FBX にある `MotionBuilder_Generic` / `FlatShader` のプロパティ・バージョン情報・接続形式をもとにしています。実行時に参照 FBX を必要としません。保存後にも、全メッシュに有効な Flat シェーダーだけが接続され、設定とバージョン情報が保持されていることを検査します。この検査に失敗した場合は `--allow-check-errors` を指定しても保存しません。

`inspect` と `check` の JSON では、`scene.shaders` と各 `scene.meshes[].shaders` でシェーダーを確認できます。`check --require-flat-shader` はこのツールの Flat 設定との一致も必須にします。通常の `check` では Flat を必須にしません。

Chis-A の修正候補は Windows / MotionBuilder 2026 で Flat の復元と模様の表示を確認済みです。透明部分の全方向・重なり順や、他モデル・他バージョンの表示は未確認です。CLI 自身は MotionBuilder を実行しないため、生成レポートの `motionbuilder_verified` は false のままです。個別ファイルの実機での確認結果は [Windows の検証結果](windows-motionbuilder-results.md) に記録しています。

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

FBX は一時ファイルへ書き出し、SDK で開き直して検査してから出力先へ移動します。`--external-textures` で途中に失敗した場合、コピー済みの `.assets` が残ることがあります。`motionbuilder_verified` は、この Mac での検査だけでは true にしません。

## テスト

```sh
python3 scripts/build.py --tests
build/native-tests
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 scripts/validate_models.py --force
python3 scripts/validate_models.py --flat-shader --output data/output/flat-shader --force
```

モデル検証は `data/input/*.fbx` と、存在すれば `data/expected/*_MB.fbx` を使います。通常の出力先は `data/output/`、詳細レポートは `data/output/reports/` です。最後のコマンドは Flat 適用版を `data/output/flat-shader/` に出力し、保存後の Flat 設定も検査します。入力ファイルの SHA-256 が処理前後で一致することも確認します。
