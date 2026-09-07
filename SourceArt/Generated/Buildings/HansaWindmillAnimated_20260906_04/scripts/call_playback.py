from unreal_ops import Client,P
from mcp_client import rpc
import json,sys
c=Client();out,_=rpc('tools/call',{'name':'list_toolsets','arguments':{}},c.sid,2);(P/'scripts/toolsets.json').write_text(json.dumps(out,indent=2))
raw=out['result']['content'][0]['text'];d=[{'name':line[2:].split(':')[0]} for line in raw.splitlines() if line.startswith('- ')]
def names(v):
 if isinstance(v,dict):
  if 'name' in v and 'HansaWindmillPlaybackTools' in str(v['name']):yield v['name']
  for x in v.values():yield from names(x)
 elif isinstance(v,list):
  for x in v:yield from names(x)
found=list(names(d));assert found,raw[:1000];name=found[0]
schema,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':name}},c.sid,3);(P/'scripts/schema_TowerTools.json').write_text(json.dumps(schema,indent=2))
identity=c.call(name,'identity');print(identity);ident=json.loads(identity);assert str(P.parents[2]/'Hansa.uproject').replace('\\','/').lower()==ident['project'].replace('\\','/').lower();(P/'engine_identity.json').write_text(identity)
if len(sys.argv)>1:print(c.call(name,sys.argv[1]))
