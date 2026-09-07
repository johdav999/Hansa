from mcp_client import rpc,P
import json
_,sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaMill','version':'1'}});rpc('notifications/initialized',sid=sid,ident=None)
sets=['EditorToolset.EditorAppToolset']+['editor_toolset.toolsets.'+a+'.'+b for a,b in [('asset','AssetTools'),('scene','SceneTools'),('static_mesh','StaticMeshTools'),('material','MaterialTools'),('object','ObjectTools'),('texture','TextureTools'),('programmatic','ProgrammaticToolset')]]
for ts in sets:
 out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':ts}},sid,3)
 (P/'scripts'/('schema_'+ts.split('.')[-1]+'.json')).write_text(json.dumps(out,indent=2));print(ts, str(out)[:150])
