"""Create reviewed-size PNG variants from genuine ImageGen masters (user-authorized resampling)."""
import json, shutil
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
SIZES = (16,20,24,28,32,40,48,56,64,80,96,112,160)
source = ROOT / 'SourceArt/UI/Icons'
shipping = ROOT / 'Content/Hansa/UI/Icons'
review = ROOT / 'Docs/Images/UI/AllMenus/Review'
shipping.mkdir(parents=True,exist_ok=True)
review.mkdir(parents=True,exist_ok=True)
data = json.loads((source/'generation-progress.json').read_text())
records, rejected = [], []
for item in data['icons']:
    original = Path(item['path'])
    if not original.is_absolute(): original = ROOT / original
    im = Image.open(original)
    if im.mode != 'RGBA' or im.getchannel('A').getextrema() != (0,255):
        rejected.append(dict(name=item['name'],path=str(original),reason='Missing genuine alpha'))
        continue
    # Ignore near-invisible generator specks when finding transparent margins.
    bbox = im.getchannel('A').point(lambda v: 255 if v >= 16 else 0).getbbox()
    if bbox:
        bbox = (max(0,bbox[0]-4),max(0,bbox[1]-4),min(im.width,bbox[2]+4),min(im.height,bbox[3]+4))
    if not bbox:
        rejected.append(dict(name=item['name'],reason='Empty alpha'));continue
    name=item['name']
    master=source/f'icons--{name.lower()}--default--{im.width}x{im.height}--v1.png'
    if original.resolve()!=master.resolve(): shutil.copy2(original,master)
    crop=im.crop(bbox)
    variants=[]
    for side in SIZES:
        # Resize premultiplied alpha to prevent black/white edge fringes.
        ratio=(side-2)/max(crop.size)
        dims=tuple(max(1,round(x*ratio)) for x in crop.size)
        art=crop.convert('RGBa').resize(dims,Image.Resampling.LANCZOS).convert('RGBA')
        out=Image.new('RGBA',(side,side));out.alpha_composite(art,((side-dims[0])//2,(side-dims[1])//2))
        path=shipping/f'{name}--{side}.png';out.save(path)
        variants.append(str(path.relative_to(ROOT)).replace('\\','/'))
    record=dict(name=name,mode='built-in ImageGen',native_size=list(im.size),crop=list(bbox),master=str(master.relative_to(ROOT)).replace('\\','/'),variants=variants,prompt=item['prompt'],alpha='RGBA',qa='Pending visual display-size review')
    records.append(record)
    master.with_suffix('.prompt.md').write_text(f"# {name} GUI icon\n\n- Generator: built-in ImageGen; model not exposed\n- Intended use: shared GUI {name} icon\n- Requested size: 32x32 or closest supported native size\n- Native size: {im.width}x{im.height}\n- Alpha crop: {bbox}; bounds at alpha >=16 plus four source pixels, retains antialiased edges\n- Display variants: {SIZES} pixels square, aspect preserved\n- Resampling: premultiplied-alpha Lanczos, authorized 2026-09-10\n- State: default artwork; native focus, hover, selected, disabled surfaces\n- Style anchor: Coin icon family; prompt matched, reference attachment produced rejected checkerboards\n- Revision: v1\n\n## Final prompt\n\n{item['prompt']}\n\n## QA\n\nOriginal displayed by ImageGen; alpha validated. Actual-size review pending.\n")
(source/'manifest.json').write_text(json.dumps(dict(assets=records,rejected=rejected),indent=2))
# Contact sheets are QA composites of separately generated assets, never generation sources.
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',12)
for page in range((len(records)+17)//18):
    subset=records[page*18:(page+1)*18]
    sheet=Image.new('RGB',(1060,55+len(subset)*62),'#F2E9D8');draw=ImageDraw.Draw(sheet)
    draw.text((12,8),'Hansa generated icons: native display pixels, linen and navy',font=font,fill='#202628')
    for row,r in enumerate(subset):
        y=40+row*62;draw.text((8,y+16),r['name'],font=font,fill='#202628')
        for col,side in enumerate((16,20,24,32,48)):
            x=115+col*90
            art=Image.open(shipping/f"{r['name']}--{side}.png")
            sheet.paste(art,(x,y+(52-side)//2),art)
            x+=465;draw.rectangle((x-4,y-2,x+62,y+54),fill='#152A35')
            sheet.paste(art,(x,y+(52-side)//2),art)
    sheet.save(review/f'icons-actual-size-{page+1}.png')
print(json.dumps(dict(accepted=len(records),rejected=rejected,review_pages=(len(records)+17)//18)))
