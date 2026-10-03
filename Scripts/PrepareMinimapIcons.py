"""Prepare genuine ImageGen minimap artwork using the approved GUI resizing exception."""
import json
import shutil
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceArt/UI/Minimap'
REVIEW = ROOT / 'Docs/Images/UI/Minimap'
RUNTIME = ROOT / 'Content/Hansa/UI/Icons'
GENERATED = Path.home() / '.codex/generated_images/01a0e673-1df9-7f70-9fea-abed4e48a2dc'
SIZES = (16,20,24,28,32,40,48,56,64,80,96,112,160)
REVIEW.mkdir(parents=True, exist_ok=True)
records = []
for entry in json.loads((SOURCE / 'generation.json').read_text()):
    original = GENERATED / entry['file']
    im = Image.open(original)
    reference = entry['name'] == 'Reference'
    dest = REVIEW if reference else SOURCE
    master = dest / f"minimap--{entry['name'].lower()}--default--{im.width}x{im.height}--v1.png"
    shutil.copy2(original, master)
    bbox = None
    if not reference:
        assert im.mode == 'RGBA' and im.getchannel('A').getextrema() == (0,255)
        bbox = im.getchannel('A').point(lambda v: 255 if v >= 16 else 0).getbbox()
        bbox = (max(0,bbox[0]-4),max(0,bbox[1]-4),min(im.width,bbox[2]+4),min(im.height,bbox[3]+4))
        crop = im.crop(bbox)
        for side in SIZES:
            ratio = (side-2) / max(crop.size)
            dims = tuple(max(1,round(v*ratio)) for v in crop.size)
            art = crop.convert('RGBa').resize(dims,Image.Resampling.LANCZOS).convert('RGBA')
            out = Image.new('RGBA',(side,side))
            out.alpha_composite(art,((side-dims[0])//2,(side-dims[1])//2))
            out.save(RUNTIME / f"{entry['name']}--{side}.png")
    record = dict(name=entry['name'],native_size=im.size,crop=bbox,master=str(master.relative_to(ROOT)),sizes=[] if reference else SIZES)
    records.append(record)
    master.with_suffix('.prompt.md').write_text(f"# {entry['name']}\n\n- Mode: built-in ImageGen; model not exposed\n- Class: {'composed visual reference only' if reference else 'runtime Slate PNG icon master'}\n- Native dimensions: {im.width}x{im.height}\n- Requested: {'1024x1024' if reference else '160x160 or closest supported square'}\n- Alpha crop: {bbox}; alpha >=16 bounds plus four source pixels; excludes invisible specks\n- Display sizes: {record['sizes']}\n- Resampling: proportional premultiplied-alpha Lanczos under approved GUI exception; original preserved\n- Style anchor: user minimap screenshot, existing Map master, and composed minimap reference\n- State: default artwork; hover/pressed/selected/disabled/focus/warning/error handled by native shared controls; loading not applicable\n- Revision: v1, zoom looking glasses and distinct center target\n\n## Final prompt\n\n{entry['prompt']}\n\n## Inspection\n\nOriginal inspected: correct subject, clear silhouette, no text/watermark, safe margins. Alpha validated for icons. See actual-size-review.png and README.md for display-size and runtime verification.\n",encoding='utf-8')
(SOURCE/'manifest.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
sheet=Image.new('RGB',(520,230),'#152A35')
draw=ImageDraw.Draw(sheet)
for row,name in enumerate(('ZoomIn','ZoomOut','CenterMap','Map','Eye')):
    y=8+row*44
    draw.text((8,y+10),name,fill='#FAF7EF')
    for col,side in enumerate((16,20,24,28,32)):
        art=Image.open(RUNTIME/f'{name}--{side}.png')
        sheet.paste(art,(100+col*75,y+(36-side)//2),art)
sheet.save(REVIEW/'actual-size-review.png')
print(json.dumps(records,indent=2))
