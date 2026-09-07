from mcp_client import rpc,P
import json
SETS={'asset':'editor_toolset.toolsets.asset.AssetTools','scene':'editor_toolset.toolsets.scene.SceneTools','mesh':'editor_toolset.toolsets.static_mesh.StaticMeshTools','material':'editor_toolset.toolsets.material.MaterialTools','object':'editor_toolset.toolsets.object.ObjectTools','app':'EditorToolset.EditorAppToolset','texture':'editor_toolset.toolsets.texture.TextureTools'}
class Client:
 def __init__(self):
  _,self.sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaMill','version':'1'}});rpc('notifications/initialized',sid=self.sid,ident=None)
 def call(self,short,name,args=None):
  out,_=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':SETS.get(short,short),'tool_name':name,'arguments':args or {}}},self.sid,5)
  if 'error' in out or out.get('result',{}).get('isError'):raise RuntimeError(json.dumps(out))
  txt=out['result']['content'][0].get('text','{}')
  try:d=json.loads(txt)
  except:return txt
  if isinstance(d,dict) and d.get('isError'):raise RuntimeError(str(d))
  return d.get('returnValue',d) if isinstance(d,dict) else d
