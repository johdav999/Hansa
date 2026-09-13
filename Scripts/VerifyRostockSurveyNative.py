"""Record reopened survey readback and run only the two fail-closed editor guard tests."""
import json
from RostockTerrainSession import REPO, JOB, call

GROUP = 'HansaEditor.HansaCityTerrainToolset'
TESTS = 'AutomationTestToolset.AutomationTestToolset'
EXPECTED_MAP = '/Game/Hansa/Generated/Staging/RostockTerrain_P31_20260908/L_Rostock_Survey_WP'


def read(method):
    return json.loads(call(GROUP, method))


context = read('InspectAuthoringContext')
assert context['projectFile'].replace('\\','/').lower() == str(REPO/'Hansa.uproject').replace('\\','/').lower()
assert context['map'] == EXPECTED_MAP and not context['pieRunning'] and not context['dirtyPackages']
survey = read('InspectRostockSurveyDraft')
assert survey['ok'] and survey['componentsLoaded'] == 256 and survey['worldPartition']
call(TESTS, 'DiscoverTests')
names = ['Hansa.Editor.Terrain.ContextIsReadOnly','Hansa.Editor.Terrain.DirtyWorkBlocksImport']
results = json.loads(call(TESTS, 'RunTests', testNames=names))
(JOB/'evidence/rostock-native-guard-tests.json').write_text(json.dumps(results,indent=2))
assert results['passed'] == 2 and results['failed'] == 0, results
after = read('InspectAuthoringContext')
assert after == context, (context, after)
record = {'context':context, 'nativeSurveyReadback':survey, 'guardTests':results,
          'reopenedInFreshEditor':True, 'productionAccepted':False}
(JOB/'evidence/rostock-native-survey-reopened.json').write_text(json.dumps(record,indent=2))
print(json.dumps(record,indent=2))
