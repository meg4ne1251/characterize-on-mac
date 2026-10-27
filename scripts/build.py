#!/usr/bin/env python3
"""Build the standalone macOS tools. No Python FBX module or Maya needed."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sdk_root():
    configured = os.environ.get("FBXSDK_ROOT")
    candidates = [Path(configured)] if configured else [
        ROOT / ".local/fbxsdk/2020.3.11",
        Path("/Applications/Autodesk/FBX SDK/2020.3.11"),
    ]
    for path in candidates:
        if (path / "include/fbxsdk.h").is_file():
            return path.resolve()
    raise SystemExit("FBX SDK が見つかりません。docs/setup.md を参照し FBXSDK_ROOT を設定してください。")


def build(tests=False):
    sdk = sdk_root()
    out = ROOT / "build"
    out.mkdir(exist_ok=True)
    source = ROOT / "src/characterize"
    library = sdk / "lib/clang/release/libfbxsdk.a"
    if not library.is_file():
        raise SystemExit(f"SDK ライブラリがありません: {library}")
    main = ROOT / "tests/native.cpp" if tests else source / "main.cpp"
    target = out / ("native-tests" if tests else "characterize")
    command = ["xcrun", "clang++", "-std=c++17", "-O2", "-Wall", "-Wextra",
               "-Wno-unused-parameter", "-Wno-deprecated-declarations",
               "-isystem", str(sdk / "include"), "-I", str(source),
               str(source / "scene.cpp"), str(source / "tpose.cpp"), str(source / "shaders.cpp"), str(source / "media.cpp"), str(main),
               str(library), "-framework", "CoreFoundation", "-liconv", "-lz", "-lxml2",
               "-o", str(target)]
    subprocess.run(command, check=True, cwd=ROOT)
    print(target)
    return target


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tests", action="store_true")
    args = parser.parse_args()
    try:
        build(args.tests)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"Build failed: {exc}", file=sys.stderr)
        sys.exit(1)
