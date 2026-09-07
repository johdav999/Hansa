from unreal_ops import Client,P
from mcp_client import rpc
import json,base64,math
c=Client();out,_=rpc('tools/call',{'name':'list_toolsets','arguments':{}},c.sid,2);raw=out['result']['content'][0]['text'];tool=next(line[2:].split(':')[0] for line in raw.splitlines() if line.startswith('- ') and 'HansaWindmillPlaybackTools:' in line)
loc=(1000,1900,1200);target=(0,0,900);d=[target[i]-loc[i] for i in range(3)];pose={'location':dict(zip('xyz',loc)),'rotation':{'pitch':math.degrees(math.atan2(d[2],math.hypot(*d[:2]))),'yaw':math.degrees(math.atan2(d[1],d[0])),'roll':0}}
c.call('app','SetCameraTransform',{'transform':pose});c.call('app','SelectActors',{'actors':[]});folder=P/'renders/unreal_animation';folder.mkdir(exist_ok=True);frames=[]
def extract(v):
 if isinstance(v,dict):
  if isinstance(v.get('data'),str) and v.get('mimeType','').startswith('image/'):return v['data']
  for k,w in v.items():
   if k=='text' and isinstance(w,str):
    try:w=json.loads(w)
    except ValueError:continue
   found=extract(w)
   if found:return found
 elif isinstance(v,list):
  for w in v:
   found=extract(w)
   if found:return found
for i in range(180):
 state=json.loads(c.call(tool,'runtime_sample'))
 out,_=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':'EditorToolset.EditorAppToolset','tool_name':'CaptureViewport','arguments':{'captureTransform':pose,'annotations':{'gridSpacing':0,'gridExtent':0,'gridHeight':0,'maxLabelDistance':0,'classFilter':None,'maxLabels':0},'bShowUI':False}}},c.sid,9)
 data=extract(out);assert data;path=folder/f'{i:03}.png';path.write_bytes(base64.b64decode(data));frames.append({'file':path.name,'time':state['time'],'state':state})
 if i>1 and state['time']-frames[0]['time']>=10:break
assert len(frames)>10 and frames[-1]['time']-frames[0]['time']>=10
(P/'unreal_recording.json').write_text(json.dumps(frames,indent=2));lines=[]
for a,b in zip(frames,frames[1:]):lines.extend([f"file '{a['file']}'",f"duration {b['time']-a['time']:.8f}"])
lines.append(f"file '{frames[-1]['file']}'");(folder/'frames.txt').write_text('\n'.join(lines));(P/'renders/unreal_animated_preview.png').write_bytes((folder/frames[len(frames)//2]['file']).read_bytes());print('RECORDED',len(frames),frames[-1]['time']-frames[0]['time'])
