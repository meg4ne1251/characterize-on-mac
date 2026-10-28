"""Run model validation from a different working directory using relative paths."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ValidationScriptTests(unittest.TestCase):
    def test_relative_paths_respect_callers_working_directory(self):
        fixtures = ROOT / 'build/script-test-data'
        subprocess.run([str(ROOT / 'build/native-tests'), str(fixtures)], cwd=ROOT,
                       check=True, capture_output=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            folder = Path(temporary)
            (folder / 'input').mkdir()
            (folder / 'expected').mkdir()
            shutil.copyfile(fixtures / 'apose.fbx', folder / 'input/Test_Miku.fbx')
            shutil.copyfile(fixtures / 'mapping-reference.fbx', folder / 'expected/Test_Miku_MB.fbx')
            result = subprocess.run(['python3', str(ROOT / 'scripts/validate_models.py'),
                                     '--input', 'input', '--expected', 'expected', '--output', 'output'],
                                    cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            summary = json.loads((folder / 'output/reports/summary.json').read_text())
            self.assertTrue(summary[0]['ok'])
            self.assertTrue(summary[0]['source_unchanged'])
