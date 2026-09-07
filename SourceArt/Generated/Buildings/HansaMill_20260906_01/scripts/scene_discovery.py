from unreal_ops import Client,P
from mcp_client import rpc
import json
c=Client();ts='editor_toolset.toolsets.actor.ActorTools';out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':ts}},c.sid,2);(P/'scripts/schema_ActorTools.json').write_text(json.dumps(out))
d=json.loads(out['result']['content'][0]['text'])
for t in d['tools']:
 if t['name'].split('.')[-1] in ['get_components','set_actor_transform','get_actor_transform']:print(t['name'],json.dumps(t['inputSchema']))
