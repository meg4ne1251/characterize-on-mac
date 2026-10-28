# Windows / MotionBuilder の表示検証手順

macOS で生成した FBX を Windows / MotionBuilder で開き、テクスチャとシェーダーの表示を確認する手順です。過去の Chis-A の不具合と修正結果は [Windows の検証結果](windows-motionbuilder-results.md) を参照してください。

## 準備

- MotionBuilder のバージョンと、確認する FBX の SHA-256 を記録します。
- 比較する入力・参照 FBX と画像は自身で用意します。`data/` のモデル・画像は同梱していません。
- 出力は macOS 側で作成します。`scripts/build.py` と `build/characterize` は Windows 用ではありません。
- 正常に表示される参照 FBX がある場合は、同じ環境で開いて基準にします。

macOS での生成例:

```sh
build/characterize configure data/input/Model.fbx \
  --output data/output/Model_mobu.fbx --name Model \
  --textures data/textures/Model --correct-tpose --flat-shader \
  --report tmp/Model-mobu.json

build/characterize check data/output/Model_mobu.fbx \
  --require-embedded-textures --require-flat-shader
```

既存出力を置換する場合は `--force` を付けます。古い画像接続も削除して適用し直す場合だけ `--reset-textures` を追加します。このオプションは透明度・法線などの画像接続も削除します。

## 実機での確認

1. 新しいシーンへ File > Open で出力 FBX を開き、開いた直後の表示を記録します。
2. Textures + Shaders 表示でも顔・髪・服などの模様と透明部分を確認します。参照 FBX は別の新しいシーンで比較します。
3. Navigator の Materials / Textures / Videos で Diffuse Texture と Video の接続、画像の寸法、展開画像の存在を確認します。
4. UVSet が対象メッシュの UV 名と一致するか、UseMaterial、モデルの ShadingMode、Flat シェーダーの有効状態と接続を調べます。
5. 表示に問題がある場合は、作業用コピーで設定を一つずつ変えて原因を切り分けます。正常な設定を FBX に保存し、CLI 出力と比較します。
6. FBX 単体を別フォルダへコピーし、元画像や既存の `.fbm` キャッシュに頼らず再度開いて表示を確認します。

画像の埋め込み復元と SDK の検査が合格しても、MotionBuilder の実表示は別途確認します。CLI の `motionbuilder_verified` は false のままです。

## 読み取り用の診断例

以下は MotionBuilder 内で実行する例です。対象バージョンの API に合わせて調整してください。macOS の CLI テストでは実行していません。

```python
from pyfbsdk import FBSystem, FBMaterialTextureType
import os

for material in FBSystem().Scene.Materials:
    texture = material.GetTexture(FBMaterialTextureType.kFBMaterialTextureDiffuse)
    video = texture.Video if texture else None
    print("MATERIAL", material.LongName)
    print("  TEXTURE", texture.LongName if texture else None)
    print("  VIDEO", video.LongName if video else None)
    if video:
        filename = getattr(video, "Filename", "")
        print("  FILE", filename, "exists", os.path.isfile(filename))
        print("  SIZE", getattr(video, "Width", None), getattr(video, "Height", None))
```

## 結果の記録

MotionBuilder のバージョン、FBX のハッシュ、スクリーンショット、画像・UV・シェーダーの検査結果、FBX 単体で移動した結果を記録します。HIK やリターゲットの検証結果は表示の確認とは分けて記載してください。

API の参照先は Autodesk の [Materials and Textures](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder-SDK/Scene-Elements/FBMaterial-and-FBTexture.html)、[FBVideoClip](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder-SDK/cpp_ref/class_f_b_video_clip.html) です。
