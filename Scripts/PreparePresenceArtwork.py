"""Documented proportional GUI display variants from retained ImageGen masters."""
from pathlib import Path
import hashlib
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Content/Hansa/UI/Presence'
OUT.mkdir(parents=True, exist_ok=True)
records = []
for name in ('harbor-site', 'coin-purse'):
    master = next((ROOT / 'SourceArt/UI/Presence').glob(f'presence--{name}--default--*--v1.png'))
    with Image.open(master) as image:
        record = {'name': name, 'master': master.relative_to(ROOT).as_posix(),
                  'native': list(image.size), 'mode': image.mode, 'crop': None,
                  'generator': 'built-in ImageGen', 'variants': []}
        if name == 'coin-purse':
            assert image.mode == 'RGBA' and image.getchannel('A').getextrema() == (0, 255), 'Purse requires genuine alpha'
            record['alpha_bounds'] = image.getchannel('A').getbbox()
        for size in (64, 96, 128, 192, 256, 384):
            height = round(image.height * size / image.width)
            path = OUT / f'{name}--{size}.png'
            image.resize((size, height), Image.Resampling.LANCZOS).save(path)
            record['variants'].append({'path': path.relative_to(ROOT).as_posix(),
                                      'size': [size, height], 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        records.append(record)
(ROOT / 'SourceArt/UI/Presence/manifest.json').write_text(json.dumps(records, indent=2)+'\n', encoding='utf-8')
print(json.dumps([{k: r[k] for k in ('name', 'native', 'mode')} for r in records]))
