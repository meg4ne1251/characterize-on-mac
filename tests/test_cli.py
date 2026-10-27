"""Integration tests against actual FBX SDK binary fixtures, never mocked pyfbsdk."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build/characterize'
FIXTURES = ROOT / 'build/test-data'
OUT = ROOT / 'build/cli-test-data'


class CliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        OUT.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(ROOT / 'build/native-tests'), str(FIXTURES)], check=True, cwd=ROOT, capture_output=True)

    def run_cli(self, *args, expected=0):
        p = subprocess.run([str(BINARY), *map(str, args)], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(p.returncode, expected, p.stderr + p.stdout[:1000])
        result = json.loads(p.stdout)
        return result

    def configure(self, target, *extra, expected=0):
        return self.run_cli('configure', FIXTURES / 'apose.fbx', '--name', 'Test_Miku',
                            '--output', OUT / target, '--textures', FIXTURES,
                            '--correct-tpose', '--force', *extra, expected=expected)

    def test_success_and_source_unchanged(self):
        before = hashlib.sha256((FIXTURES / 'apose.fbx').read_bytes()).hexdigest()
        r = self.configure('success.fbx')
        self.assertTrue(r['ok'])
        self.assertTrue(r['output_written'])
        self.assertFalse(r['motionbuilder_verified'])
        self.assertEqual(before, hashlib.sha256((FIXTURES / 'apose.fbx').read_bytes()).hexdigest())
        check = self.run_cli('check', OUT / 'success.fbx')
        self.assertTrue(check['ok'])
        self.assertEqual(len(check['scene']['characters'][0]['links']), 73)
        self.assertTrue(all(p['degrees'] < 0.001 for p in check['pose_angles']))
        self.assertTrue((OUT / 'success.assets').is_dir())

    def test_pose_correction_required(self):
        r = self.run_cli('configure', FIXTURES / 'apose.fbx', '--name', 'Test_Miku',
                         '--output', OUT / 'no-correction.fbx', '--force', expected=2)
        self.assertFalse(r['output_written'])
        self.assertIn('pose.not_tpose', {f['code'] for f in r['findings']})

    def test_reject_input_overwrite(self):
        self.run_cli('configure', FIXTURES / 'apose.fbx', '--output', FIXTURES / 'apose.fbx',
                     '--name', 'Test_Miku', '--force', expected=1)

    def test_reject_existing_output(self):
        path = OUT / 'existing.fbx'
        path.write_bytes(b'preserve this existing output')
        self.run_cli('configure', FIXTURES / 'apose.fbx', '--output', path,
                     '--name', 'Test_Miku', expected=1)
        self.assertEqual(path.read_bytes(), b'preserve this existing output')

    def test_reject_invalid_character_name(self):
        r = self.run_cli('configure', FIXTURES / 'apose.fbx', '--output', OUT / 'invalid-name.fbx',
                         '--name', 'character', '--force', expected=2)
        self.assertFalse(r['output_written'])

    def test_reject_duplicate_texture(self):
        folder = OUT / 'duplicate-images'
        folder.mkdir(exist_ok=True)
        for name in ['Body.png', 'Body.jpg']:
            shutil.copyfile(FIXTURES / 'Body.png', folder / name)
        r = self.run_cli('configure', FIXTURES / 'apose.fbx', '--output', OUT / 'ambiguous.fbx',
                         '--name', 'Test_Miku', '--correct-tpose', '--textures', folder, '--force', expected=2)
        self.assertFalse(r['output_written'])
        self.assertIn('texture.ambiguous', {f['code'] for f in r['findings']})

    def test_reject_animation(self):
        r = self.run_cli('configure', FIXTURES / 'animated.fbx', '--output', OUT / 'animated.fbx',
                         '--name', 'Test_Miku', '--correct-tpose', '--force', expected=2)
        self.assertFalse(r['output_written'])
        self.assertIn('pose.animation', {f['code'] for f in r['findings']})

    def test_rename_existing_character(self):
        r = self.run_cli('configure', FIXTURES / 'rename.fbx', '--output', OUT / 'renamed.fbx',
                         '--name', 'Test_Miku', '--rename-from-character', '--replace-character',
                         '--textures', FIXTURES, '--force')
        self.assertTrue(r['output_written'])
        self.assertEqual(r['scene']['characters'][0]['links']['Spine1'], 'Spine2')

    def test_existing_character_requires_explicit_replace(self):
        r = self.run_cli('configure', FIXTURES / 'configured.fbx', '--output', OUT / 'replace.fbx',
                         '--name', 'Test_Miku', '--force', expected=2)
        self.assertFalse(r['output_written'])
        self.assertIn('character.exists', {f['code'] for f in r['findings']})

    def test_report_is_valid_json(self):
        self.configure('report.fbx', '--report', OUT / 'report.json')
        r = json.loads((OUT / 'report.json').read_text())
        self.assertTrue(r['ok'])
        self.assertTrue(r['output_written'])

    def test_inspect_processed_and_raw(self):
        raw = self.run_cli('inspect', FIXTURES / 'apose.fbx')
        processed = self.run_cli('inspect', FIXTURES / 'configured.fbx')
        self.assertEqual(len(raw['scene']['characters']), 0)
        self.assertEqual(len(processed['scene']['characters']), 1)

    def test_reject_invalid_tolerance(self):
        for angle in ['nan', 'inf', '-1', '0', '90', '1junk']:
            self.run_cli('check', FIXTURES / 'configured.fbx', '--tolerance', angle, expected=1)


if __name__ == '__main__':
    unittest.main()
