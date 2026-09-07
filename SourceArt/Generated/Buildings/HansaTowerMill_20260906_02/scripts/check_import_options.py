from unreal_ops import Client,P
import json
c=Client();data=json.loads((P/'unreal_import_properties.json').read_text());data=json.loads(data) if isinstance(data,str) else data;obj=data['assetImportData'];s=c.call('object','list_properties',{'instance':obj});s=json.loads(s) if isinstance(s,str) else s;names=[n for n in s if any(x in n.lower() for x in ['vertexcolor','normal','degener','scale'])];v=c.call('object','get_properties',{'instance':obj,'properties':names});(P/'unreal_import_options.json').write_text(json.dumps(v,indent=2));print(v)
