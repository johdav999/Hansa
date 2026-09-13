"""Bounded engine relief/texel-scale correction; source ImageGen colors remain unchanged."""
import unreal,json
from pathlib import Path
from editor_toolset.toolsets.material import MaterialTools
JOB=Path(__file__).resolve().parents[1]
ROOT="/Game/Mesh/hansa-brewery-huexstrasse128"
for family in ("BrickWall","BrickFace","DarkBrick","Oak"):
    material=unreal.load_asset(ROOT+"/R05/Materials/M_Brewery_"+family+"_R05")
    nodes=MaterialTools.get_expressions(material)
    normal=unreal.MaterialEditingLibrary.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
    assert normal, family
    multiply=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionMultiply,-150,400)
    strength=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector,-400,500)
    normalize=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionNormalize,30,400)
    strength.set_editor_property("constant",unreal.LinearColor(.12,.12,1,1))
    strength.set_editor_property("desc","Production review: restrained tangent-space microrelief")
    assert unreal.MaterialEditingLibrary.connect_material_expressions(normal,"",multiply,"A")
    assert unreal.MaterialEditingLibrary.connect_material_expressions(strength,"",multiply,"B")
    assert unreal.MaterialEditingLibrary.connect_material_expressions(multiply,"",normalize,"VectorInput")
    assert unreal.MaterialEditingLibrary.connect_material_property(normalize,"",unreal.MaterialProperty.MP_NORMAL)
    if family=="BrickWall":
        uv=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate,-800,0)
        uv.set_editor_property("u_tiling",4.0)
        uv.set_editor_property("v_tiling",4.0)
        uv.set_editor_property("desc","Production review: wall brick repeat at strategy-building scale")
        for sample in nodes:
            if isinstance(sample,unreal.MaterialExpressionTextureSample):
                assert unreal.MaterialEditingLibrary.connect_material_expressions(uv,"",sample,"UVs")
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
unreal.log("BREWERY_RELIEF_CORRECTION_SAVED")
unreal.SystemLibrary.quit_editor()
