import sys,json,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'hansa-windmill-animation_20260906_04/scripts'))
from mcp_client import rpc
P=pathlib.Path(__file__).resolve().parents[1]
_,sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaRoad','version':'1'}})
rpc('notifications/initialized',sid=sid,ident=None)
params={'name':sys.argv[1] if len(sys.argv)>1 else 'list_toolsets','arguments':json.loads(sys.argv[2]) if len(sys.argv)>2 else {}}
out,_=rpc('tools/call',params,sid,2)
if params['name']=='describe_toolset':
 data=json.loads(out['result']['content'][0]['text'])
 (P/'scripts'/('schema_'+params['arguments']['toolset_name'].split('.')[-1]+'.json')).write_text(json.dumps(data,indent=2))
 for t in data['tools']: print(t['name'].split('.')[-1],json.dumps(t.get('inputSchema',{})))
else: print(json.dumps(out))
