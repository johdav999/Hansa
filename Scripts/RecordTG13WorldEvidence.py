"""Record verified TG-13 logs and native captures without modifying game assets."""
from pathlib import Path
import hashlib,json,re,shutil
root=Path(__file__).resolve().parents[1]
out=root/'Docs/Development/TradeWorkspaceProduction'
art=root/'Saved/BuildArtifacts'
checks=[]
for pattern in ['*-automation-Hansa.Integration.Save','*-automation-Hansa.UI.TradeMap.Construction','*-automation-Hansa.World.Rostock.PlacementAuthoring','*-automation-Hansa.Integration.TradePresence.LeasedForeignConstruction','*-gui-repair-1280-720','*-gui-repair-1920-1080']:
    folder=sorted(art.glob(pattern))[-1]
    logs=list(folder.glob('*.log'))
    tests=[]
    for log in logs:
        text=log.read_text(errors='replace')
        rows=re.findall(r'Test Completed\. Result=\{(\w+)\} Name=\{([^}]+)\} Path=\{([^}]+)\}',text)
        for status,name,path in rows:
            if not any(t['test']==path for t in tests): tests.append({'test':path,'status':status})
    assert tests and all(t['status']=='Success' for t in tests),(folder,tests)
    if 'gui-repair' in pattern:
        width,height=(1280,720) if '1280' in pattern else (1920,1080)
        command=f'./Scripts/CaptureGuiRepair.ps1 -Configuration DebugGame -Width {width} -Height {height} -TestFilter Hansa.UI.TradeConstruction.RealViewport -P31Candidate -DisableMcp -NoZenDdc'
    else:
        command='./Scripts/RunAutomationTests.ps1 -Configuration DebugGame -SkipBuild -TestFilter '+pattern.split('-automation-')[1]+' -DisableMcp -NoZenDdc'
    checks.append({'command':command,'artifacts':folder.relative_to(root).as_posix(),'tests':tests})
caps=[]
dest=out/'TG-13-world-captures';dest.mkdir(exist_ok=True)
for p in sorted((root/'Saved/TradeWorkspace/TG13').glob('construction-*')):
    if p.suffix not in ('.png','.tsv'):continue
    q=dest/p.name;shutil.copy2(p,q)
    caps.append({'path':q.relative_to(root).as_posix(),'bytes':q.stat().st_size,'sha256':hashlib.sha256(q.read_bytes()).hexdigest()})
assert len([c for c in caps if c['path'].endswith('.png')])==14
report={'date':'2026-09-24','scope':'Requested Rostock world binding, authoritative occupancy, map/world lease overlays and save topology migration','checks':checks,'captures':caps,'capture_mode':'DebugGame offscreen native viewport, P31Candidate, UI scale 1.0; synthesized semantic input; unintended edge pan disabled and controller tick suspended during held-target capture; direct production deprojection verified','inspection':'Native-resolution browser and world captures reviewed; world label clipped behind tray corrected; ghost footprint tiles rebound to Rostock; placement camera reserves space above tray. No raster assets generated or resampled.','production_status':'Not promoted: explicit approval required by root AGENTS.md and automatic approval review. See TG-13-production-review.json.','limitations':['Normal game production quarter unavailable until approved promotion','Shipping cook/package and physical controller hardware not certified','Remote hostless city visit and broader TG-13 fidelity/localization/density gates remain outside the verified standalone journey']}
(out/'TG-13-world-evidence.json').write_text(json.dumps(report,indent=2)+'\n')
print('Recorded',sum(len(c['tests']) for c in checks),'passing tests and',len(caps)//2,'native captures.')
