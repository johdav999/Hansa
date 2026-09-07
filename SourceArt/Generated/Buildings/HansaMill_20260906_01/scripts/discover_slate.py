from unreal_ops import Client,P
from mcp_client import rpc
import json
c=Client();out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':'SlateInspectorToolset.SlateInspectorToolset'}},c.sid,2);d=json.loads(out['result']['content'][0]['text']);(P/'scripts/schema_SlateTools.json').write_text(json.dumps(d,indent=2))
for t in d['tools']:print(t['name'],json.dumps(t['inputSchema']))
