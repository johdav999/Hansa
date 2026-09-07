from mcp_client import rpc,P
import json
_,sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaMill','version':'1'}});rpc('notifications/initialized',sid=sid,ident=None)
out,_=rpc('tools/call',{'name':'list_toolsets','arguments':{}},sid,2)
(P/'scripts/toolsets.json').write_text(json.dumps(out,indent=2));print(json.dumps(out))
