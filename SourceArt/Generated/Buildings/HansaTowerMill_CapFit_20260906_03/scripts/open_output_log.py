from unreal_ops import Client
import re
c=Client();tool='SlateInspectorToolset.SlateInspectorToolset'
snapshot=c.call(tool,'Snapshot',{'ref':'','maxDepth':50,'bIncludeSourceLocations':False})
match=re.search(r'button "Output Log"[^\n]*\[ref=([^\]]+)\]',str(snapshot))
assert match, 'Output Log button must be identified before clicking'
print('Verified target:',match.group(0),flush=True)
print(c.call(tool,'Click',{'ref':match.group(1)}))
print(c.call(tool,'Snapshot',{'ref':'','maxDepth':50,'bIncludeSourceLocations':False}))
