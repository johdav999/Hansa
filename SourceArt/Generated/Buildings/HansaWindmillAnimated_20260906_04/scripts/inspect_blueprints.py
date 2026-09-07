from unreal_ops import Client,P
import json
c=Client();root='/Game/Hansa/Generated/Staging/HansaWindmillAnimated_20260906_04/'
for name in ['BP_HansaWindmill_Rotor','BP_HansaWindmill_Rotor_Clearance','BP_HansaWindmill_Animated']:
 ref={'refPath':root+name+'.'+name};c.call('object','list_properties',{'instance':ref});props=c.call('object','get_properties',{'instance':ref,'properties':['staticMeshComponent']});props=json.loads(props) if isinstance(props,str) else props;component=props['staticMeshComponent'];c.call('object','list_properties',{'instance':component});print(name,c.call('object','get_properties',{'instance':component,'properties':['staticMesh']}))
