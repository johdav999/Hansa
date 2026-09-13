"""Retain verified P18 approval evidence; does not import, bind or mutate Unreal assets."""
import hashlib, json, shutil
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
job = repo/'Saved/GenerationJobs/hansa-road_P18_20260908'
approval = repo/'Saved/GenerationJobs/approved-p18-20260908'
def read(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest().upper()
def package(path): return repo/(path.replace('/Game/', 'Content/')+'.uasset')
def latest(root, pattern):
    candidates = [p for p in (repo/root).glob(pattern) if read(p).get('Status') == 'Succeeded']
    assert candidates, (root, pattern)
    return sorted(candidates)[-1]

tests = read(job/'evidence/final-regressions.json')
assert tests['passed'] == 18 and tests['failed'] == 0
assert read(job/'evidence/final-manifest-tests.json')['failed'] == 0
assert read(job/'evidence/ground-detail-tests.json')['failed'] == 0
current = read(repo/'Saved/CurrentCatalog.json')
old = read(repo/'Tests/Golden/economic_catalog_v6.json')
assert current['registryHash'] == '22248A11101B32B0' and len(current['definitions']) == 72
old_defs = {d['stableId']:d for d in old['definitions']}
assert [d['stableId'] for d in current['definitions'] if d['contentHash'] != old_defs[d['stableId']]['contentHash']] == ['Building.Road']
family = read(approval/'plan.json')[0]
before = family['definitions'][0]['before']
bound = read(approval/'bound-DA_Building_Road.json')
assert bound['authoredRevision'] == 2
for key, value in before.items():
    if key not in ('presentationActorClass','presentationMesh','authoredRevision','contentHash'):
        assert bound[key] == value, key
dependencies = read(approval/'P18-dependencies.json')
assert len(dependencies) == 11
assert not any('/Staging/' in d or '/Developer/' in d for a in dependencies for d in a['dependencies'])
shipping = latest('Saved/P18-ShippingAudit', '*/result.json')
cook = latest('Saved/P18-CookAudit', '*-media-shipping-cook/result.json')
audit = latest('Saved/P18-CookAudit', '*-media-shipping-audit/result.json')
audit_data = read(audit)
assert audit_data['ProductionReferencesAudited'] and audit_data['CookedContentAudited'] and not audit_data['Failures']
cooked = Path(read(cook)['CookedRoot'])
assets = []
for a in family['assets']:
    cooked_asset = cooked/'Hansa'/(a['destination'].replace('/Game/', 'Content/')+'.uasset')
    assert cooked_asset.is_file(), cooked_asset
    assets.append({'source':a['source'], 'destination':a['destination'], 'class':a['class'],
                   'sourceSha256':sha(package(a['source'])), 'sha256':sha(package(a['destination'])),
                   'cookedSha256':sha(cooked_asset)})
evidence = job/'evidence/release'
evidence.mkdir(exist_ok=True)
for source, name in [(shipping,'shipping-exclusion.json'),(cook,'cook-result.json'),
                     (audit,'cooked-audit.json'),(audit.parent/'references.json','production-references.json'),
                     (cook.parent/'Cook.log','Cook.log')]:
    shutil.copy2(source, evidence/name)
for config in ('DebugGame','Development'):
    log = repo/f'Saved/P18-{config}-Build.log'
    assert 'Result: Succeeded' in log.read_text(encoding='utf-8-sig',errors='replace')
receipt = {'schemaVersion':1,'prompt':'P18',
    'approval':'Contiune and implement MPVP-P18 so it i completed. I also approve to prompte it',
    'approvedOn':'2026-09-08','productionRoot':'/Game/Mesh/hansa-dirt-road',
    'status':'completed-promoted-current-mvp-contract',
    'catalogVersion':7,'registryHash':current['registryHash'],'economicsUnchanged':True,
    'compatibility':'new-game-required','previousRegistryHash':old['registryHash'],
    'stagingDependenciesFound':0,'assets':assets,
    'definitions':[{'stableId':'Building.Road','path':'/Game/Hansa/Core/Buildings/DA_Building_Road',
                    'authoredRevision':2,'contentHash':'64FB950BBE107411',
                    'sha256':sha(package('/Game/Hansa/Core/Buildings/DA_Building_Road'))}],
    'groundAmendment':'Final source/package hashes include 75 cm land datum and 10 cm shore-rise material amendment; initial plan retained as history.',
    'validation':{'debugGameEditorBuild':'passed','developmentEditorBuild':'passed',
        'targetedAutomation':{'passed':18,'failed':0,'testsWithWarnings':sum(bool(t['warnings']) for t in tests['tests'])},
        'manifestAfterUpdate':'passed','groundDetailNative1920x1080':'passed and inspected',
        'shippingBinary':'passed','productionReferences':'passed','cookedPackages':'passed; all eleven road packages present',
        'finalIoStoreContainerAudited':False,
        'evidenceRoot':'SourceArt/Generated/Roads/HansaRoad_P18_20260908/evidence'},
    'limitations':['Current MVP ground and three shore boxes only; not arbitrary Landscape conformance.',
       'No new weather simulation or gate rule.', 'Existing world dressing and overall MVP readiness outside P18.',
       'Actual viewport drag reports existing glyph fallback/render-thread warnings.',
       'Cooker-only metadata is recorded separately, not shipped-package evidence.']}
target = repo/'Docs/Development/Evidence/P18ApprovedPromotion-20260908.json'
target.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('P18_FINAL_RECEIPT',len(assets), 'production packages verified')
