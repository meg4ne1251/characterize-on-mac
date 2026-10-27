"""Integration tests against actual FBX SDK binary fixtures, never mocked pyfbsdk."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
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
        r = self.configure('success.fbx', '--external-textures')
        self.assertTrue(r['ok'])
        self.assertTrue(r['output_written'])
        self.assertFalse(r['motionbuilder_verified'])
        self.assertEqual(before, hashlib.sha256((FIXTURES / 'apose.fbx').read_bytes()).hexdigest())
        check = self.run_cli('check', OUT / 'success.fbx')
        self.assertTrue(check['ok'])
        self.assertEqual(len(check['scene']['characters'][0]['links']), 73)
        self.assertTrue(all(p['degrees'] < 0.001 for p in check['pose_angles']))
        self.assertTrue((OUT / 'success.assets').is_dir())
        self.assertEqual(r['texture_storage'], 'external')

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

    def test_flat_shader_roundtrip_and_preservation(self):
        source = FIXTURES / 'shaders-input.fbx'
        before = hashlib.sha256(source.read_bytes()).hexdigest()
        target = OUT / 'flat.fbx'
        result = self.run_cli('configure', source, '--name', 'Test_Miku', '--output', target,
                              '--flat-shader', '--force', '--report', OUT / 'flat.json')
        self.assertTrue(result['output_written'])
        op = next(o for o in result['operations'] if o.get('operation') == 'flat_shader')
        self.assertEqual((op['meshes'], op['materials'], op['replaced_connections']), (3, 2, 3))
        check = self.run_cli('check', target, '--require-flat-shader')
        self.assertFalse(check['motionbuilder_verified'])
        self.assertEqual(len(check['scene']['meshes']), 3)
        for mesh in check['scene']['meshes']:
            self.assertEqual(len(mesh['shaders']), 1)
            self.assertEqual(mesh['shaders'][0]['type'], 'FlatShader')
            self.assertTrue(mesh['shaders'][0]['flat_settings_match'])
        audit = subprocess.run([str(ROOT / 'build/native-tests'), '--audit', str(source), str(target)],
                               cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)
        self.assertEqual(before, hashlib.sha256(source.read_bytes()).hexdigest())
        repeated = OUT / 'flat-again.fbx'
        self.run_cli('configure', target, '--name', 'Test_Miku', '--output', repeated,
                     '--replace-character', '--flat-shader', '--force')
        again = self.run_cli('check', repeated, '--require-flat-shader')
        self.assertEqual(check['scene']['shaders'], again['scene']['shaders'])

    def test_flat_shader_is_optional(self):
        result = self.configure('without-flat.fbx')
        self.assertEqual(result['scene']['shaders'], [])
        self.assertTrue(all(m['shaders'] == [] for m in result['scene']['meshes']))
        check = self.run_cli('check', OUT / 'without-flat.fbx', '--require-flat-shader', expected=2)
        self.assertIn('shader.not_flat', {f['code'] for f in check['findings']})
        source = FIXTURES / 'shaders-input.fbx'
        original = self.run_cli('inspect', source)
        preserved = self.run_cli('configure', source, '--name', 'Test_Miku',
                                 '--output', OUT / 'preserved-shaders.fbx', '--force')
        reopened = self.run_cli('inspect', OUT / 'preserved-shaders.fbx')
        self.assertTrue(preserved['output_written'])
        self.assertEqual(original['scene']['shaders'], reopened['scene']['shaders'])
        self.assertEqual([m['shaders'] for m in original['scene']['meshes']],
                         [m['shaders'] for m in reopened['scene']['meshes']])

    def test_shader_option_command_scope(self):
        for command in ['inspect', 'check']:
            self.run_cli(command, FIXTURES / 'configured.fbx', '--flat-shader', expected=1)
        self.run_cli('inspect', FIXTURES / 'configured.fbx', '--require-flat-shader', expected=1)
        self.run_cli('configure', FIXTURES / 'configured.fbx', '--name', 'Test_Miku',
                     '--output', OUT / 'invalid-shader-option.fbx', '--require-flat-shader', expected=1)

    def test_embedded_textures_default_and_single_file_check(self):
        with tempfile.TemporaryDirectory(dir=OUT) as temporary:
            folder = Path(temporary)
            target = folder / 'embedded.fbx'
            result = self.run_cli('configure', FIXTURES / 'apose.fbx', '--name', 'Test_Miku',
                                  '--output', target, '--textures', FIXTURES, '--correct-tpose', '--flat-shader')
            self.assertTrue(result['output_written'])
            self.assertEqual(result['texture_storage'], 'embedded')
            self.assertFalse((folder / 'embedded.assets').exists())
            operation = next(o for o in result['operations'] if o.get('operation') == 'embed_textures')
            self.assertEqual(operation['images'], 1)
            transferred = folder / 'transferred'
            transferred.mkdir()
            shutil.copyfile(target, transferred / 'only.fbx')
            check = self.run_cli('check', transferred / 'only.fbx', '--require-embedded-textures', '--require-flat-shader')
            verified = next(o for o in check['operations'] if o.get('operation') == 'embedded_textures_verified')
            self.assertEqual(verified['images'], 1)
            image = Path(check['scene']['materials'][0]['diffuse_textures'][0]['file'])
            self.assertEqual(hashlib.sha256(image.read_bytes()).digest(), hashlib.sha256((FIXTURES / 'Body.png').read_bytes()).digest())

    def test_external_textures_cannot_pass_embedded_check(self):
        self.configure('external-only.fbx', '--external-textures')
        check = self.run_cli('check', OUT / 'external-only.fbx', '--require-embedded-textures', expected=2)
        self.assertIn('texture.not_embedded', {f['code'] for f in check['findings']})

    def test_texture_storage_option_command_scope(self):
        for command in ['inspect', 'check']:
            for option in ['--embed-textures', '--external-textures']:
                self.run_cli(command, FIXTURES / 'configured.fbx', option, expected=1)
        self.run_cli('inspect', FIXTURES / 'configured.fbx', '--require-embedded-textures', expected=1)
        self.configure('conflicting-storage.fbx', '--embed-textures', '--external-textures', expected=1)

    def test_missing_embedded_media_cannot_be_waived(self):
        target = OUT / 'preserve-on-embed-error.fbx'
        target.write_bytes(b'preserve existing output')
        result = self.run_cli('configure', FIXTURES / 'missing-media.fbx', '--name', 'Test_Miku',
                              '--output', target, '--allow-check-errors', '--force', expected=2)
        self.assertFalse(result['output_written'])
        self.assertIn('texture.embed_source', {f['code'] for f in result['findings']})
        self.assertEqual(target.read_bytes(), b'preserve existing output')


if __name__ == '__main__':
    unittest.main()
