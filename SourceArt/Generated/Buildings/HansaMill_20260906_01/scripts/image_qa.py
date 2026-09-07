from pathlib import Path
from PIL import Image
import json,hashlib
P=Path(__file__).resolve().parents[1];qa=[]
for folder in ['textures','exports','renders']:
 for p in (P/folder).glob('*.png'):
  with Image.open(p) as im:
   im.verify()
  with Image.open(p) as im:
   qa.append({'file':str(p.relative_to(P)),'size':list(im.size),'mode':im.mode,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
   if folder=='renders' and (p.stem.startswith('unreal_') or p.stem.startswith('r3_') or p.stem.startswith('reimport_')):
    # Native-size JPEG encoding for tool transport; PNG originals are authoritative.
    im.convert('RGB').save(p.with_suffix('.jpg'),quality=95,subsampling=0)
(P/'image_integrity.json').write_text(json.dumps(qa,indent=2));print('DECODED',len(qa),'IMAGES_WITH_NATIVE_DIMENSIONS')
