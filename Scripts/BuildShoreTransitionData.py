"""Derive shoreline distance and authored water level; this is data, not artwork."""
from pathlib import Path
import sys,json,hashlib,math
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Saved/GenerationJobs/LubeckTerrain_20260907/python-deps'))
import numpy as np
import shapely
from shapely.geometry import LineString
from PIL import Image
BASE=ROOT/'SourceArt/Terrain/Lubeck/Survey_20260907/hydrology'
from shapely.geometry import Polygon,LineString,box,mapping
from shapely.ops import unary_union,polygonize
from pyproj import Transformer

SURVEY=ROOT/'SourceArt/Terrain/Lubeck/Survey_20260907'
source=SURVEY/'hydrology/osm-water-complete.json'
data=json.loads(source.read_text())
project=Transformer.from_crs(4326,25832,always_xy=True)
def coords(g): return [project.transform(p['lon'],p['lat']) for p in g]
aoi=box(608850.5,5967914.5,612882.5,5971946.5)
polys=[]; lines=[]
for e in data['elements']:
    tags=e.get('tags',{})
    geom=e.get('geometry')
    p=None
    if e['type']=='way' and geom:
        if tags.get('natural')=='water' and len(geom)>3: p=Polygon(coords(geom))
        elif tags.get('waterway') in ('river','canal') and tags.get('tunnel') not in ('yes','culvert'):
            lines.append((e,LineString(coords(geom))))
    elif e['type']=='relation':
        outers=[LineString(coords(m['geometry'])) for m in e.get('members',[]) if m.get('role')=='outer' and m.get('geometry')]
        inners=[LineString(coords(m['geometry'])) for m in e.get('members',[]) if m.get('role')=='inner' and m.get('geometry')]
        if outers:
            p=unary_union(list(polygonize(unary_union(outers))))
            if inners:p=p.difference(unary_union(list(polygonize(unary_union(inners)))))
    if p is not None and not p.is_empty:
        p=p.buffer(0).intersection(aoi)
        if p.area>100:polys.append((e,p))
water=unary_union([p for e,p in polys])
mask=np.array(Image.open(BASE/'shore-wetness.png'))
y,x=np.nonzero(mask>0)
# Pixel centres match the existing source mask / Landscape UV transform.
wx=-201550+(x+.5)*403200/2017
wy=-201650+(y+.5)*403200/2017
pts=shapely.points(610866+wx/100,5969930-wy/100)
dist=shapely.distance(pts,water)
bodies=json.loads((BASE/'water-build.json').read_text())['bodies']
groups={}
for b in bodies:
 z=round(b['height_m']*100) if b['type']=='lake' else (355 if b['name'] in ('Wakenitz','Dükerkanal') else 90)
 p=[(610866+v[0]/100,5969930-v[1]/100) for v in b['points']]
 if b['type']=='lake':p.append(p[0])
 groups.setdefault(z,[]).append(LineString(p))
levels=sorted(groups)
d=np.stack([shapely.distance(pts,shapely.union_all(groups[z])) for z in levels])
nearest=np.array(levels)[d.argmin(axis=0)]
# Native river surface footprint: linear spline strips with interpolated full widths.
footprints=[]
for b in bodies:
 p=b['points']
 if b['type']=='lake':
  footprints.append(Polygon([(v[0],v[1]) for v in p]));continue
 for a,c in zip(p,p[1:]):
  dx,dy=c[0]-a[0],c[1]-a[1];length=math.hypot(dx,dy)
  if not length:continue
  nx,ny=-dy/length,dx/length;wa,wc=a[3]/2,c[3]/2
  footprints.append(Polygon([(a[0]+nx*wa,a[1]+ny*wa),(c[0]+nx*wc,c[1]+ny*wc),(c[0]-nx*wc,c[1]-ny*wc),(a[0]-nx*wa,a[1]-ny*wa)]))
footprint=shapely.union_all(footprints)
nativeDistance=shapely.distance(shapely.points(wx,wy),footprint)

out=np.zeros((*mask.shape,3),dtype=np.uint8);out[:,:,0]=255;out[:,:,2]=255
out[y,x,0]=np.rint(np.clip(dist/32,0,1)*255).astype('uint8')
out[y,x,1]=np.rint(nearest/400*255).astype('uint8')
out[y,x,2]=np.rint(np.clip(nativeDistance/3200,0,1)*255).astype('uint8')
Image.fromarray(out).save(BASE/'shore-transition-data.png')
record={'schema':1,'dimensions':[2017,2017],'mode':'geospatial data derivation; no image generation','channels':{'R':'distance outside OSM water / 32 metres, saturated','G':'nearest authored water-spline level / 400 centimetres','B':'distance outside authored linear water spline footprint / 32 metres, saturated'},'waterLevelsCm':levels,'worldUv':'(P.xy + float2(201550,201650))/403200','sourceSha256':{f:hashlib.sha256((BASE/f).read_bytes()).hexdigest() for f in ['osm-water-complete.json','water-build.json','shore-wetness.png']},'limitations':'Modern draft geography; 2m source grid, nearest-reach levels are an approximation near confluences. No change to measured elevation or Water actors.'}
(BASE/'shore-transition-data.json').write_text(json.dumps(record,indent=2))
print(json.dumps(record))
