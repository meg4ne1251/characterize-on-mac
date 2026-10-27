#!/usr/bin/env python3
"""Extract the official FBX SDK locally without running the macOS installer."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '2020.3.11'
URL = 'https://damassets.autodesk.net/content/dam/autodesk/www/files/fbx2020311_fbxsdk_clang_mac.pkg.tgz'
SHA256 = '77ff54eaaa16ab5e5bc9e77396b2040dd3d3f686342d68d6e3812e7b6b4b7451'
TARGET = ROOT / '.local/fbxsdk' / VERSION


def main():
    if (TARGET / 'include/fbxsdk.h').is_file() and (TARGET / 'lib/clang/release/libfbxsdk.a').is_file():
        print(f'FBX SDK ready: {TARGET}')
        return
    cache = ROOT / 'tmp'
    cache.mkdir(exist_ok=True)
    archive = cache / 'fbxsdk-mac.pkg.tgz'
    if not archive.is_file():
        subprocess.run(['curl', '--fail', '--location', '--output', str(archive), URL], check=True)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit('SDK archive checksum mismatch. Check the official download; the archive has not been extracted.')
    with tempfile.TemporaryDirectory(prefix='fbxsdk-', dir=cache) as temp:
        temp = Path(temp)
        with tarfile.open(archive, 'r:gz') as bundle:
            matches = [m for m in bundle.getmembers() if m.isfile() and m.name == 'fbx2020311_fbxsdk_clang_macos.pkg']
            if len(matches) != 1:
                raise SystemExit('Unexpected SDK archive layout.')
            package = temp / 'fbxsdk.pkg'
            with bundle.extractfile(matches[0]) as src, package.open('wb') as dst:
                shutil.copyfileobj(src, dst)
        expanded = temp / 'expanded'
        subprocess.run(['pkgutil', '--expand-full', str(package), str(expanded)], check=True)
        payload = expanded / 'Root.pkg/Payload/Applications/Autodesk/FBX SDK' / VERSION
        if not (payload / 'include/fbxsdk.h').is_file():
            raise SystemExit('Expected SDK headers not found in the package.')
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(payload, TARGET)
    print(f'FBX SDK ready: {TARGET}')
    print(f'SDK terms: {TARGET / "License.rtf"}')


if __name__ == '__main__':
    main()
