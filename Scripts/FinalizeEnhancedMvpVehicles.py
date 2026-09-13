"""Fail-closed P19 approval receipt from existing evidence, no asset mutation."""
import hashlib,json
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
APPROVAL=REPO/'Saved/GenerationJobs/approved-p19-20260908'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def package(path):return REPO/'Content'/(path.removeprefix('/Game/')+'.uasset')
def result(root,pattern):
    paths=[p for p in (REPO/root).glob(pattern) if read(p).get('Status')=='Succeeded']
    assert paths,root
    return max(paths,key=lambda p:p.stat().st_mtime)
tests=[read(JOB/'evidence'/name) for name in ('post-promotion-tests.json','catalog-save-tests.json')]
assert sum(t['passed'] for t in tests)==27 and all(t['failed']==0 for t in tests)
current=read(REPO/'Saved/CurrentCatalog.json');old=read(REPO/'Tests/Golden/economic_catalog_v7.json')
old_defs={d['stableId']:d for d in old['definitions']}
assert current['registryHash']=='6FAA28CD24E2C69E' and current['reverseOrderVerified']
assert [d['stableId'] for d in current['definitions'] if old_defs[d['stableId']]['contentHash']!=d['contentHash']]==['Vehicle.Cog','Vehicle.Wagon']
shipping=result('Saved/P19-ShippingAudit','*/result.json')
cook=result('Saved/P19-CookAudit','*-media-shipping-cook/result.json')
audit=result('Saved/P19-CookAudit','*-media-shipping-audit/result.json')
assert read(audit)['ProductionReferencesAudited'] and read(audit)['CookedContentAudited'] and not read(audit)['Failures']
family=read(APPROVAL/'plan.json')[0];deps=read(APPROVAL/'P19-dependencies.json');assert len(deps)==33
assert not any('/Staging/' in d or '/Developer/' in d or '/Developers/' in d for r in deps for d in r['dependencies'])
cooked=Path(read(cook)['CookedRoot'])
assets=[]
for item in family['assets']:
    cooked_path=cooked/'Hansa'/(item['destination'].replace('/Game/','Content/')+'.uasset')
    assert cooked_path.is_file(),cooked_path
    assets.append({'path':item['destination'],'class':item['class'],'sourceSha256':item['sha256'].upper(),'sha256':sha(package(item['destination'])),'cookedSha256':sha(cooked_path)})
definitions=[]
for entry in family['definitions']:
    actual=read(APPROVAL/('bound-'+entry['family']+'.json'))
    assert actual['authoredRevision']==2
    assert all(actual[k]==v for k,v in entry['before'].items() if k not in ('contentHash','authoredRevision','presentationMesh','presentationActorClass'))
    definition_hash=next(d['contentHash'] for d in current['definitions'] if d['stableId']==actual['stableDefinitionId'])
    definitions.append({'path':entry['path'],'stableId':actual['stableDefinitionId'],'authoredRevision':2,'contentHash':definition_hash,'sha256':sha(package(entry['path']))})
for name in ('P19-DebugGame-Build.log','P19-Development-Build-Retry.log'):
    assert 'Result: Succeeded' in (REPO/'Saved'/name).read_text(encoding='utf-8-sig',errors='replace')
receipt={'schemaVersion':1,'prompt':'EMVP-P19','approval':'Contiune and implement MPVP-P19 so it i completed. I also approve to prompte it','approvedOn':'2026-09-08',
    'status':'completed-promoted-asset-and-read-only-projection-contract','productionRoot':family['destination'],
    'catalogVersion':8,'registryHash':current['registryHash'],'previousCatalogVersion':7,'previousRegistryHash':old['registryHash'],'compatibility':'new-game-required','economicsUnchanged':True,
    'assets':assets,'definitions':definitions,'stagingDependenciesFound':0,
    'validation':{'targetedTestsPassed':27,'failed':0,'testsWithWarnings':sum(bool(t['warnings']) for r in tests for t in r['tests']),
        'debugGameEditor':'passed','developmentEditor':'passed on retry after compiler-internal C1001 in existing test unity unit',
        'shippingBinary':'passed','productionReferences':'passed','expandedCook':'passed; all 33 vehicle packages present','finalIoStoreContainerAudited':False,
        'reimport':'FBX and GLB bounds within 2 mm and UVs retained','nativeImages':'1280x720 berth, set sail, 120 m, wagon detail; inspected',
        'evidenceRoot':'SourceArt/Generated/Vehicles/HansaVehicles_P19_20260908/evidence'},
    'runtimeAmendment':'Harbor OnConstruction restores yaw 90 and X=1220 cm berth clearance; deck/water Z contract preserved.',
    'limitations':['Bremen-type game reconstruction; rig and wagon dimensions are interpretive.',
        'Cargo sacks indicate occupied cargo, not commodity identity or quantity.',
        'No horse, driver, harness animation, physical vehicle simulation or new route authority.',
        'P32/P34 still own continuous two-city movement, world-projection lifecycle and full delivery journey.',
        'Local wagon proof uses a deterministic dispatched logistics fixture; default trade scenario has no local jobs.',
        'Source/script/research artifacts and review maps are non-shipping. Overall enhanced-MVP readiness is not claimed.']}
(REPO/'Docs/Development/Evidence/P19ApprovedPromotion-20260908.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('P19_RECEIPT',len(assets),'packages; 27 tests; 2 definition-only presentation changes')
