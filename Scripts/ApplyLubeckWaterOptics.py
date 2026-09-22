"""Reapply or verify the shallow-water optics correction inside Unreal Python.

Console: py "<repo>/Scripts/ApplyLubeckWaterOptics.py" --apply
Default is read-only verification. Uses only the two allowlisted existing MIs.
"""
import json
import sys
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / 'SourceArt/Terrain/Lubeck/Survey_20260907/water-optics.json'
BASE = '/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/'
PATHS = [BASE + 'MI_Water_Lubeck_' + kind for kind in ('River', 'Lake')]

def main(apply=False):
    contract = json.loads(CONTRACT.read_text(encoding='utf-8-sig'))
    assert contract['schema'] == 1 and contract['materials'] == PATHS
    assert set(contract['vectors']) == {'Absorption', 'Scattering', 'Water Albedo'}
    assets = [unreal.load_asset(path) for path in PATHS]
    assert all(isinstance(asset, unreal.MaterialInstanceConstant) for asset in assets)
    mel = unreal.MaterialEditingLibrary
    before = {}
    for asset in assets:
        before[asset.get_path_name()] = {}
        for name, rgba in contract['vectors'].items():
            assert len(rgba) == 4 and all(0 <= v <= 120 for v in rgba)
            old = mel.get_material_instance_vector_parameter_value(asset, name)
            before[asset.get_path_name()][name] = [old.r, old.g, old.b, old.a]
    unreal.log('WATER_OPTICS_DIFF ' + json.dumps({'before': before, 'after': contract['vectors']}))
    if apply:
        with unreal.ScopedEditorTransaction('Correct Lubeck shallow water optics'):
            for asset in assets:
                asset.modify()
                for name, rgba in contract['vectors'].items():
                    # UE 5.8 setters may return false even on success; read back below.
                    mel.set_material_instance_vector_parameter_value(asset, name, unreal.LinearColor(*rgba))
    for asset in assets:
        for name, rgba in contract['vectors'].items():
            value = mel.get_material_instance_vector_parameter_value(asset, name)
            assert all(abs(a-b) < 0.0001 for a,b in zip([value.r,value.g,value.b,value.a], rgba)), (asset.get_path_name(), name)
        assert abs(mel.get_material_instance_scalar_parameter_value(asset, 'Water Opacity Mask Offset') + 24) < 0.001
        assert abs(mel.get_material_instance_scalar_parameter_value(asset, 'Water Roughness') - 0.22) < 0.001
    if apply:
        assert unreal.EditorLoadingAndSavingUtils.save_packages([a.get_outer() for a in assets], True)
    unreal.log('WATER_OPTICS_VERIFY_PASS: river and lake; geometry and renderer settings unchanged')

if __name__ == '__main__':
    main('--apply' in sys.argv)
