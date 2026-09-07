from unreal_ops import Client,P
import json
c=Client();actor=c.call('scene','find_actors',{'tag':'HansaAnimatedWindmill','name':'','collision_channels':[]})[0]
components=c.call('editor_toolset.toolsets.actor.ActorTools','get_components',{'actor':actor,'component_type':{'refPath':'/Script/Engine.ChildActorComponent'}});print(components)
for ref in components:
 c.call('object','list_properties',{'instance':ref});print(c.call('object','get_properties',{'instance':ref,'properties':['childActorClass','childActorTemplate','childActor']}))
