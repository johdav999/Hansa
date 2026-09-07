from unreal_ops import Client,P,rpc
import json
c=Client()
name='editor_toolset.toolsets.texture.TextureTools'
out,_=rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':name}},c.sid,3)
d=json.loads(out['result']['content'][0]['text']);(P/'scripts'/'schema_texture.decoded.json').write_text(json.dumps(d,indent=2))
for t in d['tools']:print(t['name'],json.dumps(t['inputSchema']))
print('staging exists',c.call('asset','exists',{'path':'/Game/Hansa/Generated/Staging/HansaBakery_20260906_01'}))
