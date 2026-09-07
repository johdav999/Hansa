from unreal_ops import Client,P,rpc
import json,base64,sys,math
c=Client();info=json.loads((P/'preview_actors.json').read_text());view=sys.argv[1] if len(sys.argv)>1 else 'Hero'
views={'Hero':((1800,2475,1710),(0,0,740)),'Shop':((900,1650,650),(190,650,240)),'Roof':((1700,-500,2000),(200,0,1200)),'Rear':((2400,-3100,2000),(0,-200,700))}
loc,tgt=views[view];delta=[tgt[i]-loc[i] for i in range(3)];pose={'location':dict(zip('xyz',loc)),'rotation':{'pitch':math.degrees(math.atan2(delta[2],math.hypot(*delta[:2]))),'yaw':math.degrees(math.atan2(delta[1],delta[0])),'roll':0}}
c.call('app','SetCameraTransform',{'transform':pose})
out,_=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':'EditorToolset.EditorAppToolset','tool_name':'CaptureViewport','arguments':{'captureTransform':pose,'annotations':{'gridSpacing':0,'gridExtent':0,'gridHeight':0,'maxLabelDistance':0,'classFilter':None,'maxLabels':0},'bShowUI':False}}},c.sid,9)
if out.get('result',{}).get('isError'):raise RuntimeError(str(out))
found=[]
def visit(v):
 if isinstance(v,dict):
  if isinstance(v.get('data'),str) and v.get('mimeType','').startswith('image/'):
   found.append((v['mimeType'],v['data']));return
  for k,w in v.items():
   if k=='text' and isinstance(w,str):
    try:visit(json.loads(w))
    except ValueError:pass
   else:visit(w)
 elif isinstance(v,list):
  for w in v:visit(w)
visit(out);assert found, str(out)[:500]
path=P/'renders'/('unreal_'+view+'.png');path.write_bytes(base64.b64decode(found[0][1]));print('CAPTURE',str(path),path.stat().st_size)
(P/'renders'/('unreal_'+view+'.camera.json')).write_text(json.dumps(pose,indent=2))


