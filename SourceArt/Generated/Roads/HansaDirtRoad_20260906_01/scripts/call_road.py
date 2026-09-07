import sys,pathlib,json
P=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P.parent/'hansa-windmill-animation_20260906_04/scripts'))
from unreal_ops import Client
from mcp_client import rpc
c=Client();out,_=rpc('tools/call',{'name':'list_toolsets','arguments':{}},c.sid,2)
lines=out['result']['content'][0]['text'].splitlines();name=next(l.split(':')[0].removeprefix('- ') for l in lines if 'HansaDirtRoadToolsV3:' in l)
if sys.argv[1]=='describe':
 out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':name}},c.sid,3);print(json.dumps(out))
else:print(c.call(name,sys.argv[1]))

