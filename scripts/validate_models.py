#!/usr/bin/env python3
"""Run the native pipeline and preservation audit on local user-supplied FBX models."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run_json(binary, *args):
    result = subprocess.run([str(binary), *map(str, args)], cwd=ROOT, capture_output=True, text=True)
    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f'{binary.name}: exit {result.returncode}: {result.stderr[-2000:]}') from exc
    return result.returncode, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=ROOT / 'data/input')
    parser.add_argument('--expected', type=Path, default=ROOT / 'data/expected')
    parser.add_argument('--output', type=Path, default=ROOT / 'data/output')
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    binary = ROOT / 'build/characterize'
    audit = ROOT / 'build/native-tests'
    if not binary.is_file() or not audit.is_file():
        raise SystemExit('先に python3 scripts/build.py と python3 scripts/build.py --tests を実行してください。')
    args.output.mkdir(parents=True, exist_ok=True)
    reports = args.output / 'reports'
    reports.mkdir(exist_ok=True)
    paths = sorted(args.input.glob('*.fbx'))
    if not paths:
        raise SystemExit(f'FBX がありません: {args.input}')
    summary = []
    for source in paths:
        print(f'Processing {source.name} ...', flush=True)
        before_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        _, state = run_json(binary, 'inspect', source)
        if 'scene' not in state:
            print(state, file=sys.stderr)
            return 1
        target = args.output / f'{source.stem}_mac.fbx'
        report_path = reports / f'{source.stem}.json'
        command = ['configure', source, '--name', source.stem, '--output', target,
                   '--correct-tpose', '--report', report_path]
        reference = args.expected / f'{source.stem}_MB.fbx'
        if reference.is_file():
            command += ['--texture-reference', reference]
        characters = state['scene']['characters']
        if len(characters) == 1:
            command += ['--rename-from-character', '--replace-character']
        elif len(characters) > 1:
            raise RuntimeError(f'{source.name}: 複数 Character の選択が必要です。個別に inspect してください。')
        if args.force:
            command += ['--force']
        status, result = run_json(binary, *command)
        row = {'model': source.name, 'output': target.name, 'ok': status == 0 and result.get('ok', False),
               'source_unchanged': before_hash == hashlib.sha256(source.read_bytes()).hexdigest(),
               'motionbuilder_verified': False}
        if result.get('output_written'):
            audit_status, preserved = run_json(audit, '--audit', source, target)
            (reports / f'{source.stem}-preservation.json').write_text(json.dumps(preserved, ensure_ascii=False, indent=2) + '\n')
            row['preservation_ok'] = audit_status == 0 and preserved['ok']
            row.update({k: preserved[k] for k in ['vertices_checked', 'shape_targets_checked', 'weights_checked']})
            row['max_pose_error_degrees'] = max(x['degrees'] for x in result['pose_angles'])
            row['character_links'] = len(result['scene']['characters'][0]['links'])
            if reference.is_file():
                _, expected = run_json(binary, 'inspect', reference)
                candidates = expected.get('scene', {}).get('characters', [])
                candidates = [c for c in candidates if c['name'].casefold() == source.stem.casefold()]
                if len(candidates) == 1:
                    expected_links = set(candidates[0]['links'])
                    actual_links = set(result['scene']['characters'][0]['links'])
                    row['missing_reference_slots'] = sorted(expected_links - actual_links)
                    row['additional_slots'] = sorted(actual_links - expected_links)
                    row['ok'] = row['ok'] and not row['missing_reference_slots']
            row['ok'] = row['ok'] and row['preservation_ok']
        row['ok'] = row['ok'] and row['source_unchanged']
        if not row['ok']:
            row['errors'] = result.get('error') or [f for f in result.get('findings', []) if f['severity'] == 'error']
        summary.append(row)
        (reports / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
        print(json.dumps(row, ensure_ascii=False), flush=True)
    return 0 if all(r['ok'] for r in summary) else 2


if __name__ == '__main__':
    sys.exit(main())
