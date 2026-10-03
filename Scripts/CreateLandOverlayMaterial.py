"""Reproducible native fill/ribbon materials; no raster/provider dependencies.

UV0 = world-grid dash phase, cross-ribbon coordinate [0,1].
UV1.x = 0 solid, 1 dashed, 2 dotted. Vertex color = semantic color/opacity.
The legacy Ground material is retained for older references.
"""
import unreal

PATH = "/Game/Hansa/UI/LandOverlay"
edit = unreal.MaterialEditingLibrary


def material(name):
    asset = unreal.EditorAssetLibrary.load_asset(f"{PATH}/{name}") if unreal.EditorAssetLibrary.does_asset_exist(f"{PATH}/{name}") else None
    if asset is None:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    assert isinstance(asset, unreal.Material)
    asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    asset.set_editor_property("two_sided", True)
    asset.set_editor_property("disable_depth_test", False)
    edit.delete_all_material_expressions(asset)
    return asset


def connect(a, pin, b, target):
    assert edit.connect_material_expressions(a, pin, b, target)


def finish(asset, color, color_pin, alpha, alpha_pin):
    exposure = edit.create_material_expression(asset, unreal.MaterialExpressionEyeAdaptationInverse, 350, 0)
    connect(color, color_pin, exposure, "LightValueInput")
    assert edit.connect_material_property(exposure, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert edit.connect_material_property(alpha, alpha_pin, unreal.MaterialProperty.MP_OPACITY)
    edit.recompile_material(asset)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log(f"Land material saved: {asset.get_path_name()}")


fill = material("M_UI_LandOverlay_Fill")
color = edit.create_material_expression(fill, unreal.MaterialExpressionVertexColor, -400, 0)
strength = edit.create_material_expression(fill, unreal.MaterialExpressionScalarParameter, -400, 200)
strength.set_editor_property("parameter_name", "FillOpacityMultiplier")
strength.set_editor_property("default_value", 1.0)
opacity = edit.create_material_expression(fill, unreal.MaterialExpressionMultiply, -100, 200)
connect(color, "A", opacity, "A")
connect(strength, "", opacity, "B")
clamp = edit.create_material_expression(fill, unreal.MaterialExpressionSaturate, 150, 200)
connect(opacity, "", clamp, "")
finish(fill, color, "", clamp, "")

ribbon = material("M_UI_LandOverlay_Ribbon")
color = edit.create_material_expression(ribbon, unreal.MaterialExpressionVertexColor, -500, 0)
uv = edit.create_material_expression(ribbon, unreal.MaterialExpressionTextureCoordinate, -500, 180)
style = edit.create_material_expression(ribbon, unreal.MaterialExpressionTextureCoordinate, -500, 350)
style.set_editor_property("coordinate_index", 1)
shader = edit.create_material_expression(ribbon, unreal.MaterialExpressionCustom, -100, 0)
shader.set_editor_property("description", "Joined ribbon: chalk core, navy backing, analytic feather and world-anchored dashes")
shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs = []
for name in ("Color", "Alpha", "UV", "Style"):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    inputs.append(entry)
shader.set_editor_property("inputs", inputs)
shader.set_editor_property("code", r"""
float side = abs(UV.y * 2.0 - 1.0);
float aa = clamp(fwidth(side), 0.015, 0.35);
float coverage = 1.0 - smoothstep(0.88-aa, 1.0, side);
float core = 1.0 - smoothstep(0.46-aa, 0.46+aa, side);
float pattern = 1.0;
if (Style.x > 0.5)
{
    // x+y grid phase is continuous at orthogonal corners and across chunks.
    float phase = frac(UV.x);
    float distanceToCentre = abs(phase - 0.5);
    float duty = Style.x > 1.5 ? 0.12 : 0.30;
    float paa = clamp(fwidth(UV.x), 0.01, 0.25);
    pattern = 1.0 - smoothstep(duty-paa, duty+paa, distanceToCentre);
}
// Linear RGB of Baltic Navy #152A35, matching the shared Hansa token.
float3 backing = float3(0.007499, 0.023153, 0.035601);
return float4(lerp(backing, Color.rgb, core), saturate(Alpha * coverage * pattern));
""")
connect(color, "", shader, "Color")
connect(color, "A", shader, "Alpha")
connect(uv, "", shader, "UV")
connect(style, "", shader, "Style")
alpha = edit.create_material_expression(ribbon, unreal.MaterialExpressionComponentMask, 150, 200)
alpha.set_editor_property("r", False)
alpha.set_editor_property("g", False)
alpha.set_editor_property("b", False)
alpha.set_editor_property("a", True)
connect(shader, "", alpha, "")
finish(ribbon, shader, "", alpha, "")
unreal.log("HANSA_LAND_MATERIALS_COMPLETE")
