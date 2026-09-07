from unreal_ops import Client,P
c=Client()
print(c.call('scene','get_current_level'))
print(c.call('SlateInspectorToolset.SlateInspectorToolset','Observe',{'ref':'','maxDepth':30}))
snapshot=c.call('SlateInspectorToolset.SlateInspectorToolset','Snapshot',{'ref':'','maxDepth':30,'bIncludeSourceLocations':False})
(P/'slate.txt').write_text(str(snapshot))
print(str(snapshot)[-18000:])
