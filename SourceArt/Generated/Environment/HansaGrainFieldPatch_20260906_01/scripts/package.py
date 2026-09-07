from pathlib import Path
import json,hashlib,shutil,subprocess
from PIL import Image
P=Path(__file__).resolve().parents[1]
R=P.parents[2]
D=R/'SourceArt/Generated/Environment/HansaGrainFieldPatch_20260906_01'
assert not D.exists(),f'Refusing existing destination {D}'
images=[]
for f in (P/'textures').glob('*.png'):
 with Image.open(f) as im:im.verify()
 with Image.open(f) as im:
  assert im.size==(1254,1254);images.append({'file':f.name,'dimensions':list(im.size),'mode':im.mode})
videos=[]
for name in ['wind','turntable']:
 f=P/'renders'/(name+'.mp4')
 subprocess.run(['ffmpeg','-v','error','-i',str(f),'-f','null','NUL'],check=True,capture_output=True)
 d=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,nb_frames,r_frame_rate:format=duration','-of','json',str(f)]));videos.append({'file':f.name,'metadata':d,'decode':'passed'})
(P/'media_qa.json').write_text(json.dumps({'textures':images,'videos':videos},indent=2))
qa=json.loads((P/'glb_verification.json').read_text());assert 'quarter_cycle_weights' in qa and qa['loop_position_error_m']<1e-4
for folder in ['exports','textures','renders','scripts','references']:
 for f in (P/folder).rglob('*'):
  if f.is_file() and f.suffix not in ['.blend1','.txt'] and '__pycache__' not in str(f):
   dest=D/f.relative_to(P);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
for f in P.iterdir():
 if f.is_file():shutil.copy2(f,D/f.name)
files=[{'path':str(f.relative_to(D)).replace('\\','/'),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in sorted(D.rglob('*')) if f.is_file()]
(D/'file_manifest.json').write_text(json.dumps(files,indent=2));(P/'file_manifest.json').write_text(json.dumps(files,indent=2))
print(json.dumps({'destination':str(D),'files':len(files),'total_bytes':sum(f['bytes'] for f in files),'textures_verified':len(images),'videos_decoded':2}))
