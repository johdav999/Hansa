"""Recreate ImageGen ink references as SVG geometry. Never resizes a raster.

The original samples are read at native resolution; contiguous ink samples become
closed vector paths. SVG viewBox supplies scalable geometry in Slate. PNG source
masters are copied byte for byte, not edited or used as shipping textures.
"""
from pathlib import Path
from PIL import Image
import hashlib, json, shutil

root = Path(__file__).resolve().parents[1]
source = root/'SourceArt/UI/Production'
dest = root/'Content/Hansa/UI/Production'
source.mkdir(parents=True, exist_ok=True)
dest.mkdir(parents=True, exist_ok=True)
references = {
    'flour': root/'SourceArt/UI/Production/production--flour--default--1254x1254--v2.png',
    'bread': root/'SourceArt/UI/Production/production--bread--default--1254x1254--v2.png',
    'worker': root/'SourceArt/UI/Production/production--compact-worker--default--1254x1254--v1.png',
    'grain': root/'SourceArt/UI/Production/production--grain--default--1254x1254--v1.png',
}
report=[]
for name, path in references.items():
    image=Image.open(path).convert('RGBA')
    w,h=image.size
    master=path
    if path.resolve() != master.resolve(): shutil.copyfile(path,master)
    pixels=image.load()
    runs=[]
    for y in range(h):
        start=None
        for x in range(w+1):
            dark=False
            if x<w:
                r,g,b,a=pixels[x,y]
                dark=a>200 and (r*299+g*587+b*114)<135000
            if dark and start is None:start=x
            if not dark and start is not None:
                runs.append((start,y,x-start));start=None
    assert runs, name
    x0=min(r[0] for r in runs)-3;y0=min(r[1] for r in runs)-3
    x1=max(r[0]+r[2] for r in runs)+3;y1=max(r[1] for r in runs)+4
    # One path with disjoint closed rectangles, preserving the original ink runs.
    data=''.join(f'M{x} {y}h{length}v1h-{length}z' for x,y,length in runs)
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="96" height="96" viewBox="{x0} {y0} {x1-x0} {y1-y0}"><path fill="#202628" d="{data}"/></svg>'
    (dest/f'{name}.svg').write_text(svg,encoding='utf-8')
    report.append(dict(good=name,source=str(master.relative_to(root)),nativeSize=[w,h],
        sha256=hashlib.sha256(master.read_bytes()).hexdigest(),vector=str((dest/f'{name}.svg').relative_to(root)),
        vectorRuns=len(runs),rasterResampled=False))
(source/'provenance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
