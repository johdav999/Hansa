"""Native, parameter-driven ground conformance; geometry comes from the existing MVP foundation."""
import json,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[1];job=repo/'Saved/GenerationJobs/hansa-road_P18_20260908'
sys.path.insert(0,str(repo/'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import call
M='editor_toolset.toolsets.material.MaterialTools';O='editor_toolset.toolsets.object.ObjectTools';A='editor_toolset.toolsets.asset.AssetTools'
root='/Game/Hansa/Generated/Staging/Road_P18' if len(sys.argv)==1 else '/Game/Mesh/hansa-dirt-road'
mat={'refPath':root+'/Materials/M_Road_Earth.M_Road_Earth'}
existing=call(M,'get_expressions',material_or_function=mat)
def props(obj,values):assert call(O,'set_properties',instance=obj,values=json.dumps(values))
def expr(name):
    cls=next(c for c in call(M,'list_expression_classes',material_or_function=mat,search=name) if c['refPath'].endswith('.MaterialExpression'+name))
    return call(M,'add_expression',material_or_function=mat,expression_class=cls)
def link(source,output,target,pin):call(M,'connect_expressions',from_expression=source,from_output_name=output,to_expression=target,to_input_name=pin)
custom=next((e for e in existing if ':MaterialExpressionCustom_' in e['refPath']),None) or expr('Custom')
inputs=['Position','GroundX','GroundY','GroundUp','Shore0','Shore1','Shore2','ShoreRotation0','ShoreRotation1','ShoreRotation2','Enabled','Alpha']
code='''struct RoadGround {
float rise(float2 p, float4 shore, float4 rotation) {
    float2 d=p-shore.xy;
    float2 q=float2(rotation.x*d.x-rotation.y*d.y,rotation.y*d.x+rotation.x*d.y);
    float distance=length(max(abs(q)-shore.zw,0));
    return rotation.z*(1-smoothstep(0,100,distance));
}};
RoadGround ground;
float2 p=float2(dot(float4(Position,1),GroundX),dot(float4(Position,1),GroundY));
float rise=max(ground.rise(p,Shore0,ShoreRotation0),max(ground.rise(p,Shore1,ShoreRotation1),ground.rise(p,Shore2,ShoreRotation2)));
return GroundUp.xyz*rise*saturate(Alpha)*Enabled;'''
current=json.loads(call(O,'get_properties',instance=custom,properties=['inputs']))['inputs']
if len(current)!=len(inputs):props(custom,{'inputs':current+[{'inputName':'None'} for _ in range(len(inputs)-len(current))]})
props(custom,{'inputs':[{'inputName':n} for n in inputs],'code':code,'outputType':'CMOT_Float3','description':'MVP ground: native land datum, 1m continuous shore approach; depressed shoulders, unchanged world-space texture density.'})
world=next(e for e in existing if ':MaterialExpressionWorldPosition_' in e['refPath'])
vertex=next(e for e in existing if ':MaterialExpressionVertexColor_' in e['refPath'])
link(world,'XYZ',custom,'Position');link(vertex,'A',custom,'Alpha')
for name in inputs[1:-2]:
    candidates=[e for e in existing if ':MaterialExpressionVectorParameter_' in e['refPath']]
    node=next((e for e in candidates if json.loads(call(O,'get_properties',instance=e,properties=['parameterName']))['parameterName']==name),None)
    if node is None:
        node=next((e for e in candidates if json.loads(call(O,'get_properties',instance=e,properties=['parameterName']))['parameterName']=='None'),None) or expr('VectorParameter')
    value={'r':float(name=='GroundX'),'g':float(name=='GroundY'),'b':float(name=='GroundUp'),'a':0}
    props(node,{'parameterName':name,'group':'MVP Ground','defaultValue':value});link(node,'RGBA',custom,name)
enable=expr('ScalarParameter');props(enable,{'parameterName':'GroundEnabled','defaultValue':0,'group':'MVP Ground'});link(enable,'',custom,'Enabled')
call(M,'connect_to_output',expression=custom,output_name='',material_property='MP_WorldPositionOffset')
props(mat,{'maxWorldPositionOffsetDisplacement':12})
call(M,'recompile',material_or_function=mat);assert call(A,'save_assets',asset_paths=[mat['refPath']])
(job/'evidence'/('ground-material-production.json' if len(sys.argv)>1 else 'ground-material-staging.json')).write_text(json.dumps({'material':mat,'code':code,'inputs':inputs,'maxDisplacementCm':12,'shoreApproachCm':100},indent=2))
print('GROUND_MATERIAL_COMPILED',root)
