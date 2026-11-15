"""v1.0.0 / 2026-11-15 21:25 JST: render provided cases with real library, no hardware."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[2]
    fixture = Path(__file__).resolve().parent
    subprocess.run([sys.executable, '-B', str(root / 'tests/run_api_regression.py')], check=True, timeout=180)
    prior = json.loads((root / '.test-output/latest_test.json').read_text(encoding='utf-8'))
    if not prior['passed']:
        raise RuntimeError('API regression did not pass')
    shim = root / prior['run'] / 'shim'
    out = root / '.test-output' / datetime.now(timezone.utc).strftime('voice_tuning_%Y%m%dT%H%M%S_%fZ')
    out.mkdir(parents=True)
    env = {**os.environ, 'ZIG_GLOBAL_CACHE_DIR': str(root / '.zig-global-cache'),
           'ZIG_LOCAL_CACHE_DIR': str(root / '.zig-cache')}
    cmd = [sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-O2', '-UNDEBUG',
           '-DARDUINO', '-DESP_PLATFORM', '-I', str(shim), '-I', str(root / 'src')]
    cmd += [str(root / 'src' / name) for name in (
        'MakoTalkEngine.cpp', 'MakoTalkImage.cpp', 'MakoTalkText.cpp', 'MakoTalkSpeaker.cpp')]
    cmd += [str(fixture / 'render.cpp'), '-o', str(out / 'render.exe')]
    result = subprocess.run(cmd, capture_output=True, env=env, timeout=180)
    (out / 'build.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('Render compile failed: ' + str(out / 'build.log'))
    result = subprocess.run([str(out / 'render.exe'), str(fixture / 'cases.txt'), str(out)],
                            capture_output=True, timeout=60)
    (out / 'execution.log').write_bytes(result.stdout + result.stderr)
    cases = [line.split('|')[0] for line in (fixture / 'cases.txt').read_text(encoding='utf-8').splitlines()
             if line and not line.startswith('#')]
    outputs = {name: out / (name + '.wav') for name in cases}
    complete = result.returncode == 0 and all(p.is_file() for p in outputs.values())
    hashes = {name: hashlib.sha256(p.read_bytes()).hexdigest() for name, p in outputs.items() if p.is_file()}
    differences = [name for name in cases if hashes.get(name) != hashlib.sha256(
        (root / 'docs/samples/voice_tuning' / (name + '.wav')).read_bytes()).hexdigest()]
    report = {'passed': complete and not differences, 'hardware_accessed': False,
              'scope': 'real library and dictionary; mocked I2S/NVS; exact provided WAV comparison',
              'case_count': len(cases), 'wav_sha256': hashes, 'different_or_missing': differences}
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(out), **report}, ensure_ascii=False, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
