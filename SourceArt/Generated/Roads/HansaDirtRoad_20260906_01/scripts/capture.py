import sys,pathlib,json,math,base64,time
P=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P.parent/'hansa-windmill-animation_20260906_04/scripts'))
from unreal_ops import Client
from mcp_client import rpc
c=Client();name=sys.argv[1] if len(sys.argv)>1 else 'Kit'
views={'Kit':((2200,-3000,3100),(0,0,0)),'Surface':((-700,-1400,300),(-900,-900,0)),'Spline':((800,-3600,1250),(700,-1850,0)),'Junction':((-1800,300,1300),(-900,1200,0))}
loc,tgt=views[name];d=[tgt[i]-loc[i] for i in range(3)];pose={'location':dict(zip('xyz',loc)),'rotation':{'pitch':math.degrees(math.atan2(d[2],math.hypot(*d[:2]))),'yaw':math.degrees(math.atan2(d[1],d[0])),'roll':0}}
c.call('app','SetCameraTransform',{'transform':pose});time.sleep(2)
out,_=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':'EditorToolset.EditorAppToolset','tool_name':'CaptureViewport','arguments':{'captureTransform':pose,'annotations':{'gridSpacing':0,'gridExtent':0,'gridHeight':0,'maxLabelDistance':0,'classFilter':None,'maxLabels':0},'bShowUI':False}}},c.sid,9)
found=[]
def visit(x):
 if isinstance(x,dict):
  if isinstance(x.get('data'),str) and x.get('mimeType','').startswith('image/'):found.append(x);return
  for k,v in x.items():
   if k=='text' and isinstance(v,str):
    try:visit(json.loads(v))
    except ValueError:pass
   else:visit(v)
 elif isinstance(x,list):
  for v in x:visit(v)
visit(out);assert found,str(out)[:400]
ext='jpg' if found[0]['mimeType']=='image/jpeg' else 'png';dest=P/'renders'/f'unreal_{name}.{ext}';dest.write_bytes(base64.b64decode(found[0]['data']));print(dest)
(P/'evidence'/f'unreal_{name}_camera.json').write_text(json.dumps(pose,indent=2))


