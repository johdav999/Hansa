import json,pathlib,sys
from mcp_client import rpc,P
SETS={'asset':'editor_toolset.toolsets.asset.AssetTools','scene':'editor_toolset.toolsets.scene.SceneTools','mesh':'editor_toolset.toolsets.static_mesh.StaticMeshTools','material':'editor_toolset.toolsets.material.MaterialTools','object':'editor_toolset.toolsets.object.ObjectTools','app':'EditorToolset.EditorAppToolset','texture':'editor_toolset.toolsets.texture.TextureTools','program':'editor_toolset.toolsets.programmatic.ProgrammaticToolset'}
class Client:
 def __init__(self):
  _,self.sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaBakery','version':'1'}});rpc('notifications/initialized',sid=self.sid,ident=None)
 def call(self,short,name,args={}):
  out,_=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':SETS.get(short,short),'tool_name':name,'arguments':args}},self.sid,5)
  if 'error' in out or out.get('result',{}).get('isError'):raise RuntimeError(json.dumps(out))
  txt=out['result']['content'][0].get('text','{}')
  try:d=json.loads(txt)
  except:return txt
  if d.get('isError'):raise RuntimeError(str(d))
  return d.get('returnValue',d)
if __name__=='__main__':
 c=Client();print(json.dumps(c.call(sys.argv[1],sys.argv[2],json.loads(sys.argv[3]) if len(sys.argv)>3 else {})))
