"""Collect TG-07 build/test/native viewport evidence; never alter source captures."""
from pathlib import Path
import hashlib, json, re, shutil, struct
from PIL import Image
root=Path(__file__).resolve().parents[1]
out=root/'Docs/Development/TradeWorkspaceProduction'
dest=out/'TG-07-captures';dest.mkdir(exist_ok=True)
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def log(p):
    text=p.read_text(encoding='utf-8-sig',errors='replace')
    tests=[dict(result=a,name=b,path=c) for a,b,c in re.findall(r'Test Completed\. Result=\{(\w+)\} Name=\{([^}]+)\} Path=\{([^}]+)\}',text)]
    return dict(file=p.relative_to(root).as_posix(),sha256=digest(p),tests=tests)
builddir=sorted((root/'Saved/BuildArtifacts').glob('*-build-HansaEditor-Win64-Development'))[-1]
build=builddir/'Build.log'
record={'prompt':'TG-07','build':{'file':build.relative_to(root).as_posix(),'sha256':digest(build),'succeeded':'Result: Succeeded' in build.read_text(errors='replace')},'tests':[],'captures':[],'viewport_runs':[],'reference':'Docs/Images/UI/TradeWorkspace/trade-workspace--schedule--reference--1536x1024--v1.png','artwork':'Existing approved ImageGen artwork reused; no new generation/import.'}
for pattern in ('*-automation-Hansa.UI.Trade','*-automation-Hansa.Integration.TradePresence','*-automation-Hansa.UI.TradeMap.CityInspector'):
    p=sorted((root/'Saved/BuildArtifacts').glob(pattern))[-1]/'UnrealEditor.log';record['tests'].append(log(p))
for d in sorted((root/'Saved/BuildArtifacts').glob('*-gui-repair-*')):
    p=d/'Unreal.log'
    if p.exists() and p.stat().st_mtime>build.stat().st_mtime and 'Hansa.UI.TradeCity.RealViewport' in p.read_text(errors='replace'):
        record['viewport_runs'].append(log(p))
for p in sorted((root/'Saved/TradeWorkspace/TG07').glob('city-*')):
    if p.suffix not in ('.png','.tsv'): continue
    if p.stat().st_mtime<build.stat().st_mtime: raise RuntimeError(f'Outdated capture: {p}')
    target=dest/p.name;shutil.copy2(p,target)
    row={'file':target.relative_to(root).as_posix(),'sha256':digest(target)}
    if p.suffix=='.png':row['native_dimensions']=struct.unpack('>II',p.read_bytes()[16:24])
    record['captures'].append(row)
ref=Image.open(root/record['reference']).convert('RGB')
actual=Image.open(dest/'city-1536x1024-scale1.0-Rostock-overview.png').convert('RGB')
assert ref.size==actual.size==(1536,1024)
pair=Image.new('RGB',(3072,1024));pair.paste(ref,(0,0));pair.paste(actual,(1536,0));pair.save(dest/'reference-left-game-right--3072x1024.png')
Image.blend(ref,actual,.5).save(dest/'reference-game-overlay50--1536x1024.png')
record['comparisons']=[{'file':p.relative_to(root).as_posix(),'sha256':digest(p)} for p in (dest/'reference-left-game-right--3072x1024.png',dest/'reference-game-overlay50--1536x1024.png')]
record['comparison_note']='Native-size diagnostic composites; reference sailing/cargo values differ from the truthful stopped New Game route. Shared workspace fidelity gaps remain; not a pixel-identical acceptance claim.'
(out/'TG-07-evidence.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'build_succeeded':record['build']['succeeded'],'tests':[x['tests'] for x in record['tests']],'viewport_runs':len(record['viewport_runs']),'capture_files':len(record['captures'])}))
