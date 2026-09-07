from unreal_ops import Client,P
import re
c=Client();tool='SlateInspectorToolset.SlateInspectorToolset'
snapshot=str(c.call(tool,'Snapshot',{'ref':'','maxDepth':50,'bIncludeSourceLocations':False}))
assert 'text "Cmd"' in snapshot
assert re.search(r'textbox[^\n]*\[ref=tb3\]',snapshot)
print(c.call(tool,'Type',{'ref':'tb3','text':'py "'+str(P/'scripts/capfit_toolset.py').replace('\\','/')+'"','submit':True}))
