"""Validate and archive native P26 evidence without altering image pixels."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
SIZES = ((1280, 720), (1920, 1080), (2560, 1440), (3440, 1440))
SCALES = (80, 100, 140)
STATES = ('directory', 'draft', 'invalid-name', 'review', 'retained', 'departed', 'delivered', 'accessible-draft', 'accessible-review')


def main():
    destination = ROOT / 'Docs/Images/UI/TradeP26/Native'
    destination.mkdir(parents=True, exist_ok=True)
    captures = []
    for width, height in SIZES:
        for scale in SCALES:
            for state in STATES:
                name = f'trade-{width}x{height}-scale{scale}-{state}'
                source = ROOT / 'Saved/P26' / f'{name}.png'
                data = source.read_bytes()
                assert data[:8] == b'\x89PNG\r\n\x1a\n', source
                assert struct.unpack('>II', data[16:24]) == (width, height), source
                semantic = source.with_suffix('.tsv')
                text = semantic.read_text(encoding='utf-16' if semantic.read_bytes()[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')
                if state == 'delivered':
                    match = re.search(r'routeId=(\d+) deliveredMilliUnits=(\d+)', text)
                    assert match and int(match[1]) > 3 and int(match[2]) > 0, semantic
                if state in ('review', 'retained'):
                    row = next(line.split('\t') for line in text.splitlines() if line.startswith('TradeMap.Creator.Activate\t'))
                    assert row[1:3] == ['1', '1'], row
                    x, y, right, bottom = map(int, row[3:7])
                    assert 0 <= x < right <= width and 0 <= y < bottom <= height and bottom-y >= 47, row
                for file in (source, semantic):
                    shutil.copy2(file, destination / file.name)
                captures.append({'file': source.name, 'width': width, 'height': height, 'uiScale': scale/100, 'state': state,
                                 'sha256': hashlib.sha256(data).hexdigest(), 'postCaptureResized': False})
    manifest = {'feature': 'EMVP-P26', 'captureMethod': 'Unreal Slate real game viewport; native input and ordinary game clock',
                'captures': captures, 'count': len(captures), 'allDeliveriesPassed': True, 'allDepartureBoundsPassed': True}
    (destination/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(f'P26 verified: {len(captures)} native images, 12 delivering routes, 24 visible departure controls; original bytes archived.')


if __name__ == '__main__':
    main()
