from unreal_ops import Client,P
from mcp_client import rpc
import json
c=Client()
for name in ['EditorToolset.EditorAppToolset','editor_toolset.toolsets.asset.AssetTools','SlateInspectorToolset.SlateInspectorToolset']:
 out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':name}},c.sid,2);d=json.loads(out['result']['content'][0]['text']);(P/'scripts'/('schema_'+name.split('.')[-1]+'.json')).write_text(json.dumps(d,indent=2))
 print(name,[(t['name'],t['inputSchema']) for t in d['tools'] if any(s in t['name'].lower() for s in ['project','read_file','dirty','snapshot','observe','type'])])
level=c.call('scene','get_current_level');print('LEVEL',level);print('DIRTY',c.call('asset','is_dirty',{'asset_path':level}))
