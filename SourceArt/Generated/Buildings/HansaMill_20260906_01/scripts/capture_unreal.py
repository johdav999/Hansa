from unreal_ops import Client,P
from mcp_client import rpc
import json,base64,sys,math
c=Client();name=sys.argv[1] if len(sys.argv)>1 else 'Hero'
views={'Hero':((780,1200,900),(0,0,540)),'Rear':((-850,-1400,820),(0,-100,500)),'Timber':((620,760,510),(160,200,460)),'Base':((450,-700,340),(0,-170,125)),'Roof':((650,800,980),(0,0,750)),'Iron':((130,570,740),(0,300,630))}
loc,tgt=views[name];d=[tgt[j]-loc[j] for j in range(3)];pose={'location':dict(zip('xyz',loc)),'rotation':{'pitch':math.degrees(math.atan2(d[2],math.hypot(*d[:2]))),'yaw':math.degrees(math.atan2(d[1],d[0])),'roll':0}}
c.call('app','SetCameraTransform',{'transform':pose})
for warmup in range(24):
 out,_=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':'EditorToolset.EditorAppToolset','tool_name':'CaptureViewport','arguments':{'captureTransform':pose,'annotations':{'gridSpacing':0,'gridExtent':0,'gridHeight':0,'maxLabelDistance':0,'classFilter':None,'maxLabels':0},'bShowUI':False}}},c.sid,9)
found=[]
def visit(v):
 if isinstance(v,dict):
  if isinstance(v.get('data'),str) and v.get('mimeType','').startswith('image/'):found.append(v['data']);return
  for k,w in v.items():
   if k=='text' and isinstance(w,str):
    try:visit(json.loads(w))
    except ValueError:pass
   else:visit(w)
 elif isinstance(v,list):
  for w in v:visit(w)
visit(out);assert found,str(out)[:300];path=P/'renders'/('unreal_'+name+'.png');path.write_bytes(base64.b64decode(found[0]));(P/'renders'/('unreal_'+name+'.camera.json')).write_text(json.dumps(pose,indent=2));print(path,path.stat().st_size)


