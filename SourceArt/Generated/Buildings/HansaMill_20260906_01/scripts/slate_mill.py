from unreal_ops import Client
import json
c=Client();s='SlateInspectorToolset.SlateInspectorToolset'
print(c.call(s,'Observe',{'ref':'','maxDepth':30}))
print(json.dumps(c.call(s,'Snapshot',{'ref':'','maxDepth':30,'bIncludeSourceLocations':False})))
