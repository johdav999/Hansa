"""Approved proportional GUI variants; preserve the genuine ImageGen RGBA master."""
from pathlib import Path
from PIL import Image
import hashlib, json

root = Path(__file__).resolve().parents[1]
master = next((root / 'SourceArt/UI/TradeOrders').glob('trade-orders--charcoal-crate--default--*--v1.png'))
out = root / 'Content/Hansa/UI/TradeOrders'
out.mkdir(parents=True, exist_ok=True)
with Image.open(master) as image:
    assert image.mode == 'RGBA' and image.getchannel('A').getextrema() == (0, 255)
    record = dict(master=master.relative_to(root).as_posix(), native=list(image.size), mode=image.mode,
                  generator='built-in ImageGen', crop=None, alpha_bounds=image.getchannel('A').getbbox(), variants=[])
    for width in (80, 112, 128, 160, 224):
        height = round(image.height * width / image.width)
        path = out / f'charcoal-crate--{width}.png'
        image.resize((width, height), Image.Resampling.LANCZOS).save(path)
        record['variants'].append(dict(path=path.relative_to(root).as_posix(), size=[width,height], sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
(root / 'SourceArt/UI/TradeOrders/runtime-artwork.json').write_text(json.dumps(record, indent=2)+'\n', encoding='utf-8')
print(json.dumps(record))
