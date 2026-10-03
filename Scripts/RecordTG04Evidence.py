"""Collect TG-04 build, test and native viewport evidence without changing game data."""
from pathlib import Path
import hashlib, json, re, shutil, struct

root=Path(__file__).resolve().parents[1]
out=root/'Docs/Development/TradeWorkspaceProduction'
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def latest(pattern,name):
    dirs=sorted((root/'Saved/BuildArtifacts').glob(pattern))
    p=dirs[-1]/name
    text=p.read_text(encoding='utf-8-sig',errors='replace')
    results=re.findall(r'Test Completed\. Result=\{(\w+)\} Name=\{([^}]+)\} Path=\{([^}]+)\}',text)
    return {'log':p.relative_to(root).as_posix(),'sha256':digest(p),'results':[{'result':r,'name':n,'path':path} for r,n,path in results]}

tests=[latest('*-automation-Hansa.UI.TradeMap','UnrealEditor.log'),latest('*-automation-Hansa.Integration.TradePresence','UnrealEditor.log'),latest('*-automation-Hansa.Integration.Save.RoundTripContinuation','UnrealEditor.log')]
captures=[]
destination=out/'TG-04-captures';destination.mkdir(exist_ok=True)
for p in sorted((root/'Saved/TradeWorkspace/TG04').glob('*')):
    if p.suffix not in ('.png','.tsv'):continue
    shutil.copy2(p,destination/p.name)
    row={'file':(destination/p.name).relative_to(root).as_posix(),'sha256':digest(p)}
    if p.suffix=='.png':row['native_dimensions']=struct.unpack('>II',p.read_bytes()[16:24])
    captures.append(row)
build=root/'Saved/TG04-build-10.log'
reference=root/'Docs/Images/UI/TradeWorkspace/trade-workspace--directory--reference--1672x941--v1.png'
record={'prompt':'TG-04','build':{'log':build.relative_to(root).as_posix(),'sha256':digest(build),'succeeded':'Result: Succeeded' in build.read_text(errors='replace'),'note':'Full non-unity editor build followed by final Hansa module link; original local build configuration restored after verification.'},'tests':tests,'captures':captures,'reference':{'file':reference.relative_to(root).as_posix(),'sha256':digest(reference),'native_dimensions':struct.unpack('>II',reference.read_bytes()[16:24]),'mode':'built-in ImageGen','class':'non-shipping reference'},'limitations':['No timed first-time-player attention study','No large-route performance benchmark','No full remote-client network UAT or physical controller session','No Shipping or TG-18 certification']}
(out/'TG-04-evidence.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'tests':[{'count':len(t['results']),'failed':sum(r['result']!='Success' for r in t['results'])} for t in tests],'captures':len(captures),'build':record['build']['succeeded']}))
