"""Preserve P18 source, review and promotion evidence; never touch another asset family."""
import hashlib,json,shutil
from pathlib import Path
repo=Path(__file__).resolve().parents[1];job=repo/'Saved/GenerationJobs/hansa-road_P18_20260908'
archive=repo/'SourceArt/Generated/Roads/HansaRoad_P18_20260908'
archive.mkdir(parents=True,exist_ok=True)
for folder in ('exports','textures','renders','evidence'):
    for source in (job/folder).rglob('*'):
        if not source.is_file() or source.suffix.lower() not in ('.png','.blend','.fbx','.glb','.json','.md','.log'):continue
        target=archive/folder/source.relative_to(job/folder);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
for source in (repo/'Scripts').glob('*EnhancedMvpRoad*.py'):
    target=archive/'scripts'/source.name;target.parent.mkdir(exist_ok=True);shutil.copy2(source,target)
for source in (repo/'Saved/GenerationJobs/approved-p18-20260908').glob('*.json'):
    target=archive/'evidence/promotion'/source.name;target.parent.mkdir(exist_ok=True);shutil.copy2(source,target)
for source in (repo/'Saved').glob('P18-*Tests/index.json'):
    shutil.copy2(source,archive/'evidence'/(source.parent.name+'.json'))
for source in (repo/'Saved').glob('P18-*-Build.log'):shutil.copy2(source,archive/'evidence'/source.name)
shutil.copy2(repo/'Saved/CurrentCatalog.json',archive/'evidence/catalog-v7.json')
receipt=repo/'Docs/Development/Evidence/P18ApprovedPromotion-20260908.json'
if receipt.exists():shutil.copy2(receipt,archive/'evidence/promotion'/receipt.name)
old=repo/'SourceArt/Generated/Roads/HansaDirtRoad_20260906_01/exports/Hansa_DirtRoad_Kit.blend'
old_hash=hashlib.sha256(old.read_bytes()).hexdigest().upper()
assert old_hash=='9F4569CE84B3A430CCF9DF0EB1A0F9F939519E6F7F6CB693B4D4A61F6931A781'
hashes={str(p.relative_to(archive)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in archive.rglob('*') if p.is_file() and p.name!='archive-hashes.json'}
(archive/'archive-hashes.json').write_text(json.dumps({'oldSourcePreservedSHA256':old_hash,'files':hashes},indent=2))
print('P18_ARCHIVED',len(hashes))
