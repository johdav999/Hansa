from unreal_ops import Client,P
import json
c=Client();seed=json.loads((P/'unreal_seed_nodes.json').read_text())
print('VC_OUTPUTS',c.call('material','get_expression_output_names',{'expression':seed['VertexColor']}))
assets=c.call('asset','find_assets',{'folder_path':'/Game/Hansa/Generated/Staging/HansaMill_20260906_01','name':''});print('EXISTING',assets);(P/'unreal_assets_before_retry.json').write_text(json.dumps(assets,indent=2))
