"""Restore source roof color layering and a non-destructive full fallback mesh."""
import unreal,json
from pathlib import Path
from editor_toolset.toolsets.material import MaterialTools
JOB=Path(__file__).resolve().parents[1]
ROOT="/Game/Mesh/hansa-brewery-huexstrasse128"
mesh=unreal.load_asset(ROOT+"/Production/SM_HansaBrewery_Production")
smes=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
settings=smes.get_nanite_settings(mesh)
settings.enabled=True
settings.explicit_tangents=True
settings.fallback_target=unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
settings.fallback_percent_triangles=1.0
settings.fallback_relative_error=0.0
smes.set_nanite_settings(mesh,settings)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
tones={"Terracotta":(.56,.29,.16),"Terracotta_Dark":(.45,.23,.14),"Terracotta_Warm":(.62,.31,.15),"Terracotta_Smoked":(.43,.27,.20),"Terracotta_Pale":(.60,.36,.23)}
for name,tone in tones.items():
    mat=unreal.load_asset(ROOT+"/R05/Materials/M_Brewery_"+name+"_R05")
    nodes=MaterialTools.get_expressions(mat)
    samples=[n for n in nodes if isinstance(n,unreal.MaterialExpressionTextureSample) and n.get_name()=="MaterialExpressionTextureSample_0"]
    assert len(samples)==1
    mul=next((n for n in nodes if isinstance(n,unreal.MaterialExpressionMultiply)),None)
    tint=next((n for n in nodes if isinstance(n,unreal.MaterialExpressionConstant3Vector)),None)
    if mul is None:mul=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionMultiply,-150,0)
    if tint is None:tint=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,150)
    tint.set_editor_property("constant",unreal.LinearColor(tone[0],tone[1],tone[2],1))
    tint.set_editor_property("desc","R05 source roof tint; restores Blender-only color layer")
    assert unreal.MaterialEditingLibrary.connect_material_expressions(samples[0],"RGB",mul,"A")
    assert unreal.MaterialEditingLibrary.connect_material_expressions(tint,"",mul,"B")
    assert unreal.MaterialEditingLibrary.connect_material_property(mul,"",unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
(JOB/"material-fallback-correction.json").write_text(json.dumps({"mesh":mesh.get_path_name(),"fallback_percent":1.0,"fallback_error":0.0,"explicit_tangents":True,"roof_tints":tones},indent=2))
unreal.log("BREWERY_MATERIAL_FALLBACK_CORRECTED")
unreal.SystemLibrary.quit_editor()
