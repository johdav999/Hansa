from unreal_ops import Client,P
import json
c=Client();level='/Game/Hansa/Developer/GenerationPreview/HansaMill_20260906_01/L_MillPreview';assert c.call('asset','exists',{'path':level});print('CURRENT',c.call('scene','get_current_level'))
assert c.call('asset','save_assets',{'asset_paths':[level]});c.call('scene','load_level',{'level_path':level})
# Continue exactly after the completed duplicate operation, never duplicate again.
s=(P/'scripts/prepare_scene.py').read_text();start=s.index("actors=c.call");scope={'c':c,'P':P,'json':json,'level':level,'cur':'/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP'};exec(s[start:],scope)
