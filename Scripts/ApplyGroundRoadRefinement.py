"""Apply only the road-earth refinement using the C++ author's exact shader source.
Used when unrelated concurrent C++ changes temporarily prevent a whole-project build.
"""
import unreal, re
from pathlib import Path
root=Path(unreal.Paths.project_dir())
source=(root/'Source/HansaEditor/Private/World/HansaGroundSurfaceAuthoring.inl').read_text(encoding='utf-8-sig')
noise=re.search(r'SurfaceNoise = TEXT\(R"HLSL\((.*?)\)HLSL"\);',source,re.S).group(1)
code=re.search(r'Earth->Code=FString\(SurfaceNoise\)\+TEXT\(R"HLSL\((.*?)\)HLSL"\);',source,re.S).group(1)
el=unreal.MaterialEditingLibrary
m=unreal.load_asset('/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain')
expressions=el.get_material_expressions(m)
find=lambda desc: next((e for e in expressions if e.get_editor_property('desc')==desc),None)
albedo=find('Hansa road albedo v2')
edge=find('Hansa broken shoulders v3')
assert albedo and edge
with unreal.ScopedEditorTransaction('Hansa road earth surface variation'):
 m.modify()
 earth=find('Hansa road earth variation v3')
 if not earth:
  original=el.get_inputs_for_material_expression(m,albedo)[0]
  edge_inputs=el.get_inputs_for_material_expression(m,edge)
  earth=el.create_material_expression(m,unreal.MaterialExpressionCustom)
  earth.set_editor_property('desc','Hansa road earth variation v3')
  earth.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
  inputs=[]
  for name in ['Base','P','Coverage']:
   i=unreal.CustomInput(); i.set_editor_property('input_name',name); inputs.append(i)
  earth.set_editor_property('inputs',inputs)
  assert el.connect_material_expressions(original,'',earth,'Base')
  assert el.connect_material_expressions(edge_inputs[0],'',earth,'P')
  assert el.connect_material_expressions(edge_inputs[1],'A',earth,'Coverage')
 earth.set_editor_property('code',noise+code)
 assert el.connect_material_expressions(earth,'',albedo,'A')
 el.recompile_material(m)
 assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
unreal.log('HANSA_ROAD_EARTH_APPLIED')
