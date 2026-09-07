from unreal_ops import Client,P,rpc
import json
c=Client();ts='editor_toolset.toolsets.actor.ActorTools';out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':ts}},c.sid,6);d=json.loads(out['result']['content'][0]['text']);(P/'scripts'/'schema_actor.decoded.json').write_text(json.dumps(d,indent=2))
for t in d['tools']:
 if any(k in t['name'] for k in ['components','transform','material_override']):print(t['name'],json.dumps(t['inputSchema']),json.dumps(t.get('outputSchema')))
