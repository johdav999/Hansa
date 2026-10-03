"""Record TG-16 native captures and verification logs without resampling images."""
from pathlib import Path
import hashlib
import json
import shutil
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / 'Saved/TradeWorkspace/TG16'
out = root / 'Docs/Development/TradeWorkspaceProduction/TG-16-captures'
out.mkdir(parents=True, exist_ok=True)

def record(path):
    item = {'path': path.relative_to(root).as_posix(), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    if path.suffix == '.png':
        with Image.open(path) as im:
            item['dimensions'] = list(im.size)
    return item

captures = []
for path in sorted(source.glob('recovery-*')):
    if path.suffix not in ('.png', '.tsv'):
        continue
    if path.suffix == '.png' and path.stem[-2:] not in ('00', '01', '02', '03', '06'):
        continue
    dest = out / path.name
    shutil.copy2(path, dest)
    captures.append(record(dest))

reference = root / 'Docs/Images/UI/TradeWorkspace/trade-workspace--recovery--reference--1536x1024--v1.png'
actual = out / 'recovery-1536x1024-scale1.0-02.png'
with Image.open(reference) as ref, Image.open(actual) as game:
    assert ref.size == game.size == (1536, 1024)
    side = Image.new('RGB', (3072, 1024))
    side.paste(ref, (0, 0))
    side.paste(game, (1536, 0))
    side.save(out / 'reference-left--game-right--3072x1024.png')
    Image.blend(ref.convert('RGB'), game.convert('RGB'), .5).save(out / 'reference-game--overlay50--1536x1024.png')

artifacts = root / 'Saved/BuildArtifacts'
def latest(pattern):
    paths = sorted(artifacts.glob(pattern))
    assert paths, pattern
    return paths[-1].relative_to(root).as_posix()

viewport = []
for name in ['20260924-095341775-gui-repair-1280-720', '20260924-095409490-gui-repair-1536-1024',
             '20260924-095437216-gui-repair-1920-1080', '20260924-095506356-gui-repair-1280-720']:
    path = artifacts / name
    log = (path / 'Unreal.log').read_text(encoding='utf-8', errors='replace')
    assert 'Hansa.UI.Recovery.RealViewport' in log and 'Result={Success}' in log and 'Result={Fail}' not in log
    viewport.append(path.relative_to(root).as_posix())

manifest = {
    'date': '2026-09-24', 'reference': record(reference),
    'capture_kind': 'Real Development game; ordinary New Game followed by explicit saved test campaign, native dimensions',
    'fidelity_accepted': False,
    'build': latest('20260924-*-build-HansaEditor-Win64-Development'),
    'tests': [latest('20260924-*-automation-Hansa.UI.TradeMap')],
    'viewport_runs': viewport,
    'capture_revision_note': 'Final header captured. Subsequent additional-lease enumeration and its regression test do not alter this single-base-lease fixture.',
    'packaged': {'accepted': False, 'log': 'Saved/TradeWorkspace/TG16/packaged-startup.log',
        'build_log': 'Saved/TG16-Package-Final.log', 'cook': 'Existing cooked content reused',
        'blocker': 'Ordinary New Game catalog fingerprint mismatch; see TG-16.md.'},
    'captures': captures,
    'comparisons': [record(out / 'reference-left--game-right--3072x1024.png'), record(out / 'reference-game--overlay50--1536x1024.png')],
    'verification_hashes': [],
}
for relative in manifest['tests'] + [manifest['build']] + viewport:
    for path in sorted((root / relative).iterdir()):
        if path.suffix in ('.log', '.json'):
            manifest['verification_hashes'].append(record(path))
for relative in [manifest['packaged']['log'], manifest['packaged']['build_log']]:
    manifest['verification_hashes'].append(record(root / relative))
for path in sorted((root / 'Docs/Images/UI/TradeWorkspace').glob('*recovery*')):
    manifest['verification_hashes'].append(record(path))
for path in sorted((root / 'Docs/Images/UI/TradeWorkspace').glob('*closure-review*')):
    manifest['verification_hashes'].append(record(path))
(out.parent / 'TG-16-evidence.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(f'Recorded {len(captures)} native files and {len(viewport)} viewport runs.')
