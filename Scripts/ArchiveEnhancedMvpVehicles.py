"""Retain P19 source and verified evidence; never modify prior source families."""
import hashlib,json,shutil,html
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
ARCHIVE=REPO/'SourceArt/Generated/Vehicles/HansaVehicles_P19_20260908'
APPROVAL=REPO/'Saved/GenerationJobs/approved-p19-20260908'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
def copy(source,target):target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
for folder in ('exports','renders','evidence'):
    for source in (JOB/folder).rglob('*'):
        if source.is_file() and source.suffix.lower() in ('.png','.blend','.fbx','.glb','.json','.md','.log'):
            copy(source,ARCHIVE/folder/source.relative_to(JOB/folder))
for name in ('oak-source','canvas-source'):
    original=REPO/'SourceArt/Generated/Buildings/HansaMarket_P15_20260908/textures'/name
    assert sha(original.with_suffix('.png'))==sha(JOB/'textures'/(name+'.png'))
    for suffix in ('.png','.prompt.md'):copy(original.parent/(name+suffix),ARCHIVE/'textures'/(name+suffix))
for source in (JOB/'textures').glob('linen-source*'):copy(source,ARCHIVE/'textures'/source.name)
for source in (JOB/'checkpoints').glob('Vehicles-r*.blend'):copy(source,ARCHIVE/'checkpoints'/source.name)
for source in (REPO/'Scripts').glob('*EnhancedMvp*Vehicle*.py'):copy(source,ARCHIVE/'scripts'/source.name)
copy(REPO/'Scripts/PromoteEnhancedMvpP19.py',ARCHIVE/'scripts/PromoteEnhancedMvpP19.py')
for source in APPROVAL.glob('*'):
    if source.is_file():copy(source,ARCHIVE/'evidence/promotion'/source.name)
for source in (REPO/'Saved').glob('P19-*-Build*.log'):copy(source,ARCHIVE/'evidence'/source.name)
for root in ('P19-ShippingAudit','P19-CookAudit'):
    for source in (REPO/'Saved'/root).glob('*/result.json'):copy(source,ARCHIVE/'evidence/release'/root/source.parent.name/'result.json')
copy(REPO/'Tests/Golden/economic_catalog_v8.json',ARCHIVE/'evidence/catalog-v8.json')
receipt=REPO/'Docs/Development/Evidence/P19ApprovedPromotion-20260908.json'
if receipt.exists():copy(receipt,ARCHIVE/'evidence/promotion'/receipt.name)
def latest(pattern):return max((JOB/'renders').glob(pattern),key=lambda p:p.stat().st_mtime).name
pairs=[('Hull archaeological reference / r4 interpretation','../../../../Saved/GenerationJobs/hansa-vehicles_P19_20260908/references/DSM-page4.png','renders/r4-cog.png'),
       ('Draft-shaft interface reference / r4 wagon','../../../../Saved/GenerationJobs/hansa-vehicles_P19_20260908/references/Langdon-page34.png','renders/r4-wagon.png'),
       ('R2 bow/deck defects / corrected r4','renders/r2-deck.png','renders/r4-deck.png'),
       ('R3 wheel UV defect / corrected r4','renders/r3-wagon.png','renders/r4-wagon.png'),
       ('Native color anchor / rendered repeated linen','textures/linen-source.png','renders/swatch-Linen.png'),
       ('R4 / clean FBX reimport','renders/r4-cog.png','renders/reimport-fbx-whole.png'),
       ('R4 / production Unreal set sail','renders/r4-cog.png','renders/'+latest('native-Underway-*.png')),
       ('R4 / production Unreal wagon','renders/r4-wagon.png','renders/'+latest('native-WagonDetail-*.png'))]
page='<!doctype html><html lang="en"><meta charset="utf-8"><title>P19 native-size comparisons</title><style>body{font:16px system-ui;margin:24px}section{overflow:auto}.pair{display:flex;gap:20px}figure{margin:0;flex:none}img{max-width:none;height:auto}figcaption{margin:8px 0}</style><h1>P19 native-size comparisons</h1><p>Reference/earlier version left, current render right. Images retain native pixels; scroll instead of rescaling. Archaeological and manuscript images have different camera, age, lighting and scale; they establish construction, not an exact surface-color match. Research pages are local private caches, not redistributable Hansa textures. Blender and Unreal exposure differ. See the gap ledger for qualified findings.</p>'
for title,left,right in pairs:
    page+='<h2>'+html.escape(title)+'</h2><section class="pair">'
    for path in (left,right):page+='<figure><figcaption>'+html.escape(path)+'</figcaption><img src="'+html.escape(path)+'" alt="'+html.escape(title)+'"></figure>'
    page+='</section>'
page+='</html>'
(ARCHIVE/'COMPARISONS.html').write_text(page,encoding='utf-8')
hashes={p.relative_to(ARCHIVE).as_posix():sha(p) for p in ARCHIVE.rglob('*') if p.is_file() and p.name!='archive-hashes.json'}
(ARCHIVE/'archive-hashes.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
print('P19_ARCHIVED',len(hashes),'files')
