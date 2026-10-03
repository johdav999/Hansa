"""Collect TG-05 logs and native viewport outputs without changing game data."""
from pathlib import Path
import hashlib,json,re,shutil,struct
root=Path(__file__).resolve().parents[1]
out=root/'Docs/Development/TradeWorkspaceProduction'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def log(pattern,name):
 matches=sorted((root/'Saved/BuildArtifacts').glob(pattern))
 if not matches:return {'missing':pattern}
 p=matches[-1]/name
 text=p.read_text(encoding='utf-8-sig',errors='replace')
 results=re.findall(r'Test Completed\. Result=\{(\w+)\} Name=\{([^}]+)\} Path=\{([^}]+)\}',text)
 return {'file':p.relative_to(root).as_posix(),'sha256':sha(p),'tests':[dict(result=a,name=b,path=c) for a,b,c in results]}
record={'prompt':'TG-05','tests':[log('*-automation-Hansa.Multiplayer.CommandPaths.AllCurrentActions','UnrealEditor.log'),log('*-automation-Hansa.UI.TradeCreator','UnrealEditor.log'),log('*-automation-Hansa.UI.TradeMap','UnrealEditor.log'),log('*-automation-Hansa.UI.TradeJourney','UnrealEditor.log')],'viewport_runs':[log('*-gui-repair-1280-720','Unreal.log'),log('*-gui-repair-1920-1080','Unreal.log')],'captures':[],'art':'Existing approved assets and native components reused; no new raster generation or imports.'}
build=sorted((root/'Saved/BuildArtifacts').glob('*-build-HansaEditor-Win64-Development'))[-1]/'Build.log'
record['build']={'file':build.relative_to(root).as_posix(),'sha256':sha(build),'succeeded':'Result: Succeeded' in build.read_text(errors='replace')}
record['module_builds']=[]
for name in ('TG05-final-runtime-build.log','TG05-final-tests-build.log'):
 p=root/'Saved'/name
 record['module_builds'].append({'file':p.relative_to(root).as_posix(),'sha256':sha(p),'succeeded':'Result: Succeeded' in p.read_text(errors='replace')})
dest=out/'TG-05-captures';dest.mkdir(exist_ok=True)
for p in sorted((root/'Saved/P26').glob('trade-*')):
 if p.suffix not in ('.png','.tsv') or 'scale100' not in p.name or p.stat().st_mtime < build.stat().st_mtime:continue
 shutil.copy2(p,dest/p.name)
 row={'file':(dest/p.name).relative_to(root).as_posix(),'sha256':sha(p)}
 if p.suffix=='.png':row['native_dimensions']=struct.unpack('>II',p.read_bytes()[16:24])
 record['captures'].append(row)
for p in sorted((root/'Saved/P34').glob('import-repeat-*.txt')):
 shutil.copy2(p,dest/p.name)
 record['captures'].append({'file':(dest/p.name).relative_to(root).as_posix(),'sha256':sha(p)})
(out/'TG-05-evidence.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'tests':[{r.get('file','missing'):r.get('tests',[])} for r in record['tests']],'capture_files':len(record['captures']),'build_succeeded':record['build']['succeeded']}))
