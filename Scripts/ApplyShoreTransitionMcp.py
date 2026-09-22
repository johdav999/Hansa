"""Apply native C++ shoreline shaders through the connected Unreal editor."""
from pathlib import Path
import sys,json,re,shutil
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'SourceArt/Generated/Trees/LubeckSummer/v1/scripts'))
from ue_batch import call
M='editor_toolset.toolsets.material.MaterialTools';O='editor_toolset.toolsets.object.ObjectTools';A='editor_toolset.toolsets.asset.AssetTools';T='editor_toolset.toolsets.texture.TextureTools'
BASE='/Game/Hansa/Generated/Staging/LubeckTerrain_20260907'
paths=[BASE+'/Materials/M_Terrain_Hansa_Master','/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground']
context=json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
assert Path(context['projectFile']).resolve()==ROOT/'Hansa.uproject' and not context['pieRunning'],context
source=(ROOT/'Source/HansaEditor/Private/World/HansaGroundSurfaceAuthoring.inl').read_text()
noise=re.search(r'SurfaceNoise = TEXT\(R"HLSL\((.*?)\)HLSL"\);',source,re.S).group(1)
def shader(name):return noise+re.search(name+r'->Code=FString\(SurfaceNoise\)\+TEXT\(R"HLSL\((.*?)\)HLSL"\);',source,re.S).group(1)
def ref(p):return {'refPath':p}
def get(obj,*names):return json.loads(call(O,'get_properties',instance=obj,properties=list(names)))
def props(obj,**values):
 call(O,'list_properties',instance=obj)
 get(obj,*values)
 assert call(O,'set_properties',instance=obj,values=json.dumps(values))
def connect(a,b,name,out=''):call(M,'connect_expressions',from_expression=a,from_output_name=out,to_expression=b,to_input_name=name)
def inputs(mat,node):return call(M,'get_expression_inputs',material_or_function=mat,expression=node)
def new(mat,kind):return call(M,'add_expression',material_or_function=mat,expression_class=ref('/Script/Engine.MaterialExpression'+kind))
texpath=BASE+'/Textures/T_Terrain_Lubeck_ShoreBands'
tex=ref(texpath+'.T_Terrain_Lubeck_ShoreBands') if call(A,'exists',path=texpath) else call(T,'import_file',folder_path=BASE+'/Textures',asset_name='T_Terrain_Lubeck_ShoreBands',source_file=str(ROOT/'SourceArt/Terrain/Lubeck/Survey_20260907/hydrology/shore-transition-data.png'))
if isinstance(tex,list):tex=tex[0]
props(tex,sRGB=False,compressionSettings='TC_VectorDisplacementmap',addressX='TA_Clamp',addressY='TA_Clamp',mipGenSettings='TMGS_NoMipmaps')
for path in paths:
 disk=ROOT/('Content'+path[5:]+'.uasset');backup=ROOT/'Saved/TerrainSurface/BeforeShore'/disk.name
 backup.parent.mkdir(parents=True,exist_ok=True)
 if not backup.exists():shutil.copy2(disk,backup)
 mat=ref(path+'.'+path.rsplit('/',1)[-1]);expr=call(M,'get_expressions',material_or_function=mat)
 bydesc={get(e,'Desc')['Desc']:e for e in expr}
 def custom(desc,names,code,typ):
  e=bydesc.get(desc)
  if not e:
   e=new(mat,'Custom');props(e,desc=desc,outputType=typ)
  if [i['input_name'] for i in inputs(mat,e)]!=names:
   props(e,inputs=[])
   props(e,inputs=[{'inputName':n} for n in names])
  props(e,code=code)
  return e
 color=bydesc['Hansa ground macro variation v3'];ci=inputs(mat,color);P=ci[1]['expression']
 uv=custom('Hansa shoreline UV v1',['P'],'return (P.xy+float2(201550.0,201650.0))/403200.0;','CMOT_Float2');connect(P,uv,'P')
 weights=bydesc.get('Hansa shoreline weights v1')
 if weights and isinstance(inputs(mat,weights)[0]['expression'],dict):sample=inputs(mat,weights)[0]['expression']
 else:
  sample=new(mat,'TextureSample');props(sample,texture=tex['refPath'],samplerType='SAMPLERTYPE_LinearColor');connect(uv,sample,'UVs')
 props(sample,texture=tex['refPath'],samplerType='SAMPLERTYPE_LinearColor')
 weights=custom('Hansa shoreline weights v1',['Data','P'],shader('W'),'CMOT_Float3');connect(sample,weights,'Data','RGB');connect(P,weights,'P')
 if len(ci)==5:
  props(color,inputs=[])
  props(color,inputs=[{'inputName':i['input_name']} for i in ci]+[{'inputName':'Shore'}])
  for i in ci:connect(i['expression'],color,i['input_name'],i['output_name'])
 connect(weights,color,'Shore');props(color,code=shader('Color'))
 rs=call(M,'get_property_input',material=mat,material_property='MP_Roughness')['expression'];ri=inputs(mat,rs);print('ROUGH',ri,flush=True)
 rb=next(i['expression'] for i in ri if i['input_name']=='Yes');base=next(i for i in ri if i['input_name']=='No')
 rough=custom('Hansa shoreline roughness v1',['Base','Shore'],'return lerp(lerp(lerp(Base,.72,Shore.r),.82,Shore.g),.32,Shore.b);','CMOT_Float1')
 if base['expression']!=rough:connect(base['expression'],rough,'Base',base['output_name'])
 connect(weights,rough,'Shore');connect(rough,rs,'No');connect(rough,rb,'A')
 call(M,'recompile',material_or_function=mat)
 assert len(inputs(mat,color))==6
 print('SHORE_COMPILED',path,flush=True)
call(A,'save_assets',asset_paths=paths+[texpath])
print('SHORE_SAVED',flush=True)
