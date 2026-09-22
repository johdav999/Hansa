"""Read-only reproducibility and geometry checks for the staged world source package."""
import json
import hashlib
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Saved/GenerationJobs/HansaWorld_20260918/python'))
import numpy as np
from scipy.ndimage import binary_erosion

SOURCE=ROOT/'SourceArt/Terrain/HansaWorld/Prototype_20260918'
manifest=json.loads((SOURCE/'terrain-manifest.json').read_text(encoding='utf-8'))
width,height=manifest['vertices'];step=manifest['world_step_m']
for record in manifest['sources']:
    with (SOURCE/'sources'/record['file']).open('rb') as stream:
        assert hashlib.file_digest(stream,'sha256').hexdigest()==record['sha256'],record['file']
for metadata_path in (SOURCE/'sources').glob('*.osm.source.json'):
    record=json.loads(metadata_path.read_text(encoding='utf-8'))
    with metadata_path.with_name(metadata_path.name.replace('.source.json','.json')).open('rb') as stream:
        assert hashlib.file_digest(stream,'sha256').hexdigest()==record['sha256'],metadata_path.name
assert (width-1)%126==0 and (height-1)%126==0
assert (width-1)*(height-1)//(126*126)<=1024
for name,expected in manifest['files'].items():
    with (SOURCE/name).open('rb') as stream:
        assert hashlib.file_digest(stream,'sha256').hexdigest()==expected,name
def read(name):
    a=np.fromfile(SOURCE/name,dtype='<u2');assert a.size==width*height
    return (a.reshape(height,width).astype('float32')-32768)*400/12800
base=read('survey-base.r16');hydro=read('hydrology-delta.r16');grading=read('gameplay-delta.r16');final=read('terrain-final.r16')
maximum_layer_error=float(np.abs(base+hydro+grading-final).max())
assert maximum_layer_error <= .047,maximum_layer_error
water=np.load(SOURCE/'water-surface.npy')
assert np.isfinite(final).all()
assert np.all(final[np.isfinite(water)]<water[np.isfinite(water)])
assert len({city['id'] for city in manifest['cities']})==31
yy,xx=np.indices((height,width),dtype='float32')
city_results=[]
for city in manifest['cities']:
    row=int(round(city['y_cm']/100/step));col=int(round(city['x_cm']/100/step))
    assert np.isnan(water[row,col]),city['name']
    assert abs(final[row,col]*100-city['z_cm'])<=1.6,city['name']
    core=(np.hypot(xx*step-city['x_cm']/100,yy*step-city['y_cm']/100)<city['core_radius_m']) & np.isnan(water)
    interior=binary_erosion(core)
    dy,dx=np.gradient(final,step)
    maximum_slope=float(np.degrees(np.arctan(np.hypot(dx[interior],dy[interior]))).max()) if interior.any() else None
    assert maximum_slope is not None and maximum_slope<=3, (city['name'],maximum_slope)
    city_results.append({'id':city['id'],'core_maximum_slope_degrees':maximum_slope,'dry_centre':True})
trees=np.fromfile(SOURCE/'trees.f32',dtype='<f4').reshape(-1,7)
assert len(trees)==manifest['tree_count']
rows=np.rint(trees[:,1]/100/step).astype(int);cols=np.rint(trees[:,0]/100/step).astype(int)
assert np.isnan(water[rows,cols]).all()
assert np.max(np.abs(trees[:,2]/100-final[rows,cols]))<.02
required={'Thames','Rhine','Elbe','Weser','Trave','Warnow','Oder','Vistula','Neman','Daugava','Narva','Neva','Göta älv','Volkhov'}
assert required.issubset(set(manifest['river_names'])),required-set(manifest['river_names'])
assert 'Преголя' in manifest['river_names']
assert not any('canal' in x.lower() for x in manifest['river_names'])
result={'source_validation':'passed','maximum_encoded_layer_sum_error_m':maximum_layer_error,
        'city_checks':city_results,'tree_count':len(trees),'components':(width-1)*(height-1)//126**2,
        'limitations':manifest['limitations'],'unreal_validation':'separate; source validation is not viewport or navigation approval'}
output=ROOT/'Saved/GenerationJobs/HansaWorld_20260918/source-validation.json'
output.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ['city_checks','limitations']},indent=2))
