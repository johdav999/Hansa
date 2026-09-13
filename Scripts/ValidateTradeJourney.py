"""Verify and archive unresampled P34 screenshots joined to state and event evidence."""
import hashlib
import json
import shutil
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
STATES = ('opportunity', 'source-report', 'import-draft', 'review', 'loading', 'departing', 'traveling', 'arriving', 'unloading', 'market-feedback', 'needs-feedback', 'cancelled')
MODES = ((1280, 720, '1.0'), (1920, 1080, '1.0'), (2560, 1440, '1.0'), (3440, 1440, '1.0'), (1280, 720, '0.8'), (1280, 720, '1.4'))

def metrics(path):
    raw = path.read_bytes()
    lines = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig').splitlines()
    result = dict(line.split('=', 1) for line in lines if '=' in line and not line.startswith(('ui=', 'event=')))
    result['domainEvents'] = [line[6:].split('|') for line in lines if line.startswith('event=')]
    return result

def main():
    source = ROOT / 'Saved/P34'
    output = ROOT / 'Docs/Images/UI/TradeJourneyP34'
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for width, height, scale in MODES:
        group = {}
        for state in STATES:
            name = f'journey-{width}x{height}-{scale}-{state}'
            png = source / (name + '.png')
            txt = source / (name + '.txt')
            with Image.open(png) as image:
                assert image.size == (width, height), png
                assert image.getchannel('A').getextrema() == (255, 255), png
            m = metrics(txt)
            group[state] = m
            rows.append(dict(state=state, width=width, height=height, scale=scale, sha256=hashlib.sha256(png.read_bytes()).hexdigest(), capture=f'Docs/Images/UI/TradeJourneyP34/{png.name}', **m))
            shutil.copyfile(png, output / png.name)
            shutil.copyfile(txt, output / txt.name)
        for state in ('loading', 'unloading'):
            m = group[state]
            assert m['transferTick'] == m['tick'] == m['worldTransferTick']
            assert m['transferMilli'] == m['worldTransfer'] == '19000'
            assert any(e[1] == m['tick'] and e[2] in ('RouteCargoTransferred', 'RouteCargoMissed') and e[3] == m['transferCity'] and e[4] == 'Good.Bread' and e[5] == m['transferMilli'] for e in m['domainEvents'])
        assert group['loading']['rostockBread'] == '5000'
        assert group['loading']['worldCargo'] == '19000'
        assert group['unloading']['worldCargo'] == '0'
        assert int(group['unloading']['lubeckBread']) > 0
        assert any(e[2] == 'RouteEdited' for e in group['cancelled']['domainEvents'])
        assert any(e[2] == 'RouteCancelled' for e in group['cancelled']['domainEvents'])
    for state in STATES:
        assert len({r['fingerprint'] for r in rows if r['state'] == state}) == 1, (state, 'resolution or scale changed simulation state')
    for repeat in (0, 1):
        shutil.copyfile(source / f'import-repeat-{repeat}.txt', output / f'import-repeat-{repeat}.txt')
    result = dict(schemaVersion=1, scope='EMVP-P34 production-v8 gameplay; staged P31 Rostock visual quarter', passed=True, nativeCaptures=len(rows), rasterResampling=False, captures=rows)
    destination = ROOT / 'Docs/Development/TradeJourneyP34/verification.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'PASS: {len(rows)} native captures, conserved cargo, matched domain events, deterministic across all displays.')

if __name__ == '__main__':
    main()
