from unreal_ops import Client,P
import re
c=Client();tool='SlateInspectorToolset.SlateInspectorToolset'
c.call(tool,'Observe',{'ref':'','maxDepth':50})
snapshot=str(c.call(tool,'Snapshot',{'ref':'','maxDepth':50,'bIncludeSourceLocations':False}))
section=snapshot[snapshot.index('text "Cmd"'):];match=re.search(r'textbox[^\n]*\[ref=([^\]]+)\]',section);assert match
print('Verified console input',match.group(0))
print(c.call(tool,'Type',{'ref':match.group(1),'text':'py "'+str(P/'scripts/final_rotor_toolset.py').replace('\\','/')+'"','submit':True}))
