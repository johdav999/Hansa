import sys,pathlib,re,json
P=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P.parent/'hansa-windmill-animation_20260906_04/scripts'))
from unreal_ops import Client
c=Client();tool='SlateInspectorToolset.SlateInspectorToolset'
c.call(tool,'Observe',{'ref':'','maxDepth':50})
snapshot=str(c.call(tool,'Snapshot',{'ref':'','maxDepth':50}))
section=snapshot[snapshot.index('text "Cmd"'):];match=re.search(r'textbox[^\n]*\[ref=([^\]]+)\]',section);assert match
print(c.call(tool,'Type',{'ref':match.group(1),'text':'py "'+str(P/'scripts/review_toolset.py').replace('\\','/')+'"','submit':True}))

