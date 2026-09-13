import json, shutil
from pathlib import Path
from PIL import Image, ImageDraw
root=Path.cwd()
metadata=json.loads((root/'SourceArt/UI/Icons/building-icon-generation.json').read_text(encoding='utf-8-sig'))
sizes=(16,20,24,28,32,40,48,56,64,80,96,112,160)
manifest_path=root/'SourceArt/UI/Icons/manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
for item in metadata:
    name=item['name']
    im=Image.open(item['source'])
    assert im.mode=='RGBA', (name,im.mode)
    alpha=im.getchannel('A')
    assert alpha.getextrema()==(0,255)
    w,h=im.size
    master=Path(f'SourceArt/UI/Icons/icons--{name.lower()}--default--{w}x{h}--v1.png')
    shutil.copy2(item['source'],root/master)
    b=alpha.point(lambda a:255 if a>=16 else 0).getbbox()
    crop=(max(0,b[0]-4),max(0,b[1]-4),min(w,b[2]+4),min(h,b[3]+4))
    cut=im.crop(crop)
    paths=[]
    for size in sizes:
        factor=(size-4)/max(cut.size)
        dims=tuple(max(1,round(v*factor)) for v in cut.size)
        resized=cut.convert('RGBa').resize(dims,Image.Resampling.LANCZOS).convert('RGBA')
        out=Image.new('RGBA',(size,size))
        out.alpha_composite(resized,((size-dims[0])//2,(size-dims[1])//2))
        dest=Path(f'Content/Hansa/UI/Icons/{name}--{size}.png')
        out.save(root/dest);paths.append(dest.as_posix())
    record=f"""# {name} building GUI icon

- Mode: built-in ImageGen; model not exposed
- Status: production PNG artwork consumed by Slate dynamic brushes
- Requested: 48x48; generated: {w}x{h}
- Original master: {master.as_posix()}
- Alpha crop: {crop}; alpha >=16 bounds expanded four pixels
- Display variants: {sizes}, square transparent canvases
- Resampling: premultiplied-alpha Lanczos, proportional fit with 2px margins
- Authorization: GUI resizing exception, 2026-09-10
- Style anchor: existing Icons family / engraved brass and natural materials
- States: artwork default; existing native hover/pressed/selected/disabled/focus/warning/error
- Revision: v1, replaces incorrect residence fallback
- QA: original inspected; genuine RGBA verified; display-size review recorded in BuildingConstructionIcons.md

## Final prompt

{item['prompt']}
"""
    (root/master.with_suffix('.prompt.md')).write_text(record,encoding='utf-8')
    manifest['assets'] = [a for a in manifest['assets'] if a['name'] != name]
    manifest['assets'].append(dict(name=name,mode='built-in ImageGen',native_size=[w,h],crop=crop,master=master.as_posix(),variants=paths,prompt=item['prompt'],alpha='RGBA',qa='Original inspected; RGBA validated; display and runtime QA in Docs/Development/BuildingConstructionIcons.md'))
    print(name,im.size,crop)
manifest_path.write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
# Diagnostic composition only: paste the genuine display variants at 1:1.
qa=Image.new('RGB',(550,342),'#152A35');draw=ImageDraw.Draw(qa)
for row,(name,label) in enumerate([('Road','Road'),('Warehouse','Warehouse'),('Dock','Dock'),('Market','Market')]):
    y=10+row*82;draw.text((10,y+15),label,fill='#F2E9D8')
    for col,size in enumerate((32,40,48,64)):
        x=105+col*105
        icon=Image.open(root/f'Content/Hansa/UI/Icons/{name}--{size}.png')
        qa.paste(icon,(x,y),icon);draw.text((x+size+3,y+12),str(size),fill='#F2E9D8')
out=root/'Docs/Images/UI/Construction/BuildingIcons'
out.mkdir(parents=True,exist_ok=True)
qa.save(out/'display-size-review.png')
