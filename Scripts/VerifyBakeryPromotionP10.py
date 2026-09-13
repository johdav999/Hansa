"""Run inside a fresh Unreal editor Python commandlet after the approved P10 migration."""
import json
from pathlib import Path
import unreal

repo = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
applied = json.loads((repo / 'Saved/GenerationJobs/hansa-bakery-p10_20260907/evidence/catalog_applied.json').read_text(encoding='utf-8-sig'))
assert applied['applied'] and applied['reverseOrderVerified']
assert applied['registryHash'] == 'DA77AC921DFB6DC0'
bakery = unreal.load_asset('/Game/Hansa/Core/Buildings/DA_Building_Bakery')
assert str(bakery.get_editor_property('presentation_mesh')).find('SM_Bakery_Body') >= 0
assert str(bakery.get_editor_property('presentation_actor_class')).find('HansaBakeryPresentation') >= 0
paths = []
for role in ('Body', 'Construction', 'Input', 'Output', 'Sign'):
    path = '/Game/Mesh/hansa-bakery/P10/Meshes/SM_Bakery_' + role
    asset = unreal.load_asset(path)
    assert asset
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'Hansa.Approval', 'Approved by user 2026-09-07; P10 bakery promotion and reviewed bread-chain migration')
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'Hansa.PresentationRole', role)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    paths.append(path)
receipt = dict(applied)
receipt['approval'] = 'Yes, I approve promoting the bakery and applying the reviewed bread-chain catalog migration'
receipt['approvalDate'] = '2026-09-07'
receipt['promotedMeshes'] = paths
receipt['shippingAcceptance'] = 'Not established by this promotion; cook and correlated gameplay-resolution evidence remain required.'
out = repo / 'Docs/Development/Evidence/P10ApprovedPromotion-20260907.json'
out.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
unreal.log('P10_APPROVED_PROMOTION_VERIFIED')
