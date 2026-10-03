from pathlib import Path
import hashlib, json, shutil
from PIL import Image
root = Path(__file__).resolve().parents[1]
source = root / 'Saved/TradeWorkspace/TG15'
out = root / 'Docs/Development/TradeWorkspaceProduction/TG-15-captures'
out.mkdir(parents=True, exist_ok=True)
records = []
for path in sorted(source.glob('visiting-*')):
    if path.suffix not in ('.png', '.tsv'): continue
    if path.suffix == '.png' and path.stem[-2:] not in ('00', '01', '04', '07'): continue
    dest = out / path.name
    shutil.copy2(path, dest)
    item = {'path': dest.relative_to(root).as_posix(), 'sha256': hashlib.sha256(dest.read_bytes()).hexdigest()}
    if path.suffix == '.png':
        with Image.open(dest) as im: item['dimensions'] = list(im.size)
    records.append(item)
reference = root / 'Docs/Images/UI/TradeWorkspace/trade-workspace--visiting-market--reference--1536x1024--v2.png'
actual = out / 'visiting-1536x1024-scale1.0-01.png'
with Image.open(reference) as ref, Image.open(actual) as game:
    assert ref.size == game.size == (1536, 1024)
    side = Image.new('RGB', (3072, 1024))
    side.paste(ref, (0, 0)); side.paste(game, (1536, 0))
    side.save(out / 'reference-left--game-right--3072x1024.png')
    Image.blend(ref.convert('RGB'), game.convert('RGB'), .5).save(out / 'reference-game--overlay50--1536x1024.png')
artifacts = root / 'Saved/BuildArtifacts'
viewport = []
for path in sorted(artifacts.glob('20260924-091*-gui-repair-*')):
    log = (path / 'Unreal.log').read_text(encoding='utf-8', errors='replace')
    if 'Hansa.UI.VisitingTrade.RealViewport' in log and 'Result={Success}' in log and 'Result={Fail}' not in log:
        viewport.append(path.relative_to(root).as_posix())
manifest = {
    'date': '2026-09-24', 'reference': reference.relative_to(root).as_posix(),
    'capture_kind': 'Real assembled Development game, explicit saved test campaign after ordinary New Game; not Shipping or generated imagery',
    'comparisons': 'Native-size diagnostic compositions without resampling. Reference-fidelity gate remains open; see TG-15.md.',
    'build': 'Saved/BuildArtifacts/20260924-091054003-build-HansaEditor-Win64-Development',
    'tests': ['Saved/BuildArtifacts/' + name for name in [
        '20260924-090750088-automation-Hansa.UI.VisitingTrade.Journey',
        '20260924-090805044-automation-Hansa.UI.TradeMap',
        '20260924-090552302-automation-Hansa.UI.Market',
        '20260924-090132364-automation-Hansa.Integration.TradePresence.VisitingSpotTrade']],
    'viewport_runs': viewport,
    'packaged': {'accepted': False, 'log': 'Saved/TradeWorkspace/TG15/packaged-startup.log',
        'blocker': 'Ordinary New Game rejects loaded catalog EE2F7D53434867C9 against accepted 2A9D09E1C63AA6E1; residential definitions mismatch before visiting UI.'},
    'captures': records,
}
manifest['verification_hashes'] = []
for relative in manifest['tests'] + [manifest['build']] + manifest['viewport_runs']:
    directory = root / relative
    for path in sorted(directory.glob('*')):
        if path.suffix not in ('.log', '.json'): continue
        manifest['verification_hashes'].append({'path': path.relative_to(root).as_posix(), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
for relative in ['Saved/TG15-Package-Final.log', manifest['packaged']['log']]:
    path = root / relative
    if path.exists():
        manifest['verification_hashes'].append({'path': relative, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
(out.parent / 'TG-15-evidence.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(f'Recorded {len(records)} files and {len(viewport)} successful viewport runs.')
