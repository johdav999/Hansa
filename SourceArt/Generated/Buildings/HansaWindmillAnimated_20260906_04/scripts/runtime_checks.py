from unreal_ops import Client,P
from mcp_client import rpc
import json,time,math
c=Client();out,_=rpc('tools/call',{'name':'list_toolsets','arguments':{}},c.sid,2);raw=out['result']['content'][0]['text'];name=next(line[2:].split(':')[0] for line in raw.splitlines() if line.startswith('- ') and 'HansaWindmillPlaybackTools:' in line)
def sample():return json.loads(c.call(name,'runtime_sample'))
results=[]
for rate in [36,0,-36,36]:
 c.call(name,'set_test_speed',{'degrees_per_second':rate});a=sample();time.sleep(1.4);b=sample();dt=b['time']-a['time'];assert dt>0
 q=a['rotor']['quaternion'];r=b['rotor']['quaternion'];angle=2*math.degrees(math.acos(min(1,abs(sum(x*y for x,y in zip(q,r))))));expected=abs(rate)*dt
 assert abs(angle-expected)<1, (angle,expected)
 assert max(abs(x-y) for x,y in zip(a['rotor']['location'],b['rotor']['location']))<.001
 assert a['building']==b['building'];assert b['active'] and b['updated_component']=='StaticMeshComponent0'
 signed_y=q[3]*r[1]-q[1]*r[3]
 if rate:assert signed_y*rate<0
 results.append({'rate_deg_s':rate,'elapsed_game_seconds':dt,'measured_degrees':angle,'expected_degrees':expected,'pivot_fixed':True,'body_fixed':True,'direction_verified':True,'before':a,'after':b})
(P/'runtime_verification.json').write_text(json.dumps(results,indent=2));print('RUNTIME_ROTATION_PAUSE_REVERSE_PASSED')
