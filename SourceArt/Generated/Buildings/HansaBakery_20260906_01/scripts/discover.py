import sys,json,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from mcp_client import rpc,P
_,sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaBakery','version':'1'}});rpc('notifications/initialized',sid=sid,ident=None)
for short,name in [('asset','editor_toolset.toolsets.asset.AssetTools'),('scene','editor_toolset.toolsets.scene.SceneTools'),('material','editor_toolset.toolsets.material.MaterialTools'),('object','editor_toolset.toolsets.object.ObjectTools'),('app','EditorToolset.EditorAppToolset'),('mesh','editor_toolset.toolsets.static_mesh.StaticMeshTools'),('program','editor_toolset.toolsets.programmatic.ProgrammaticToolset')]:
 out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':name}},sid,3)
 (P/'scripts'/('schema_'+short+'.json')).write_text(json.dumps(out,indent=2))
 print(short,json.dumps(out)[:160])
