from pathlib import Path
P=Path(__file__).resolve().parents[1];old=P.parent/'hansa-mill_20260906_01/scripts'
s=(old/'bake_export.py').read_text().replace('mill_r3.blend','tower_r3.blend').replace('HansaMill','HansaTowerMill').replace("names=['Oak','RoofTimber','Limestone','ForgedIron','ShelteredOak']","names=['Masonry','WeatheredTimber','SagePaint','OldBrick','ForgedIron','WindowGlass','RecessTimber','Fieldstone']").replace('1254','1024').replace(".62 if rec['name']=='ForgedIron'", ".55 if rec['name']=='ForgedIron'")
start=s.index('for o in asset:\n bpy.context.view_layer.objects.active=o\n for mod')
end=s.index('bpy.context.view_layer.objects.active=asset[0];bpy.ops.object.join()',start)
s=s[:start]+"bpy.context.view_layer.objects.active=asset[0];bpy.ops.object.convert(target='MESH')\n"+s[end:]
s=s.replace("'uv_coverage_m':1", "'uv_coverage_m':2.5 if m.name=='Masonry' else 1")
s=s.replace("'Procedural iron: no image detail needed'", "'Procedural small trim/metal/glass/recess material; no photographic texture required'")
s=s.replace("'base_color_px_per_m':1024", "'base_color_px_per_m':409.6 if m.name=='Masonry' else 1024")
(P/'scripts/bake_export.py').write_text(s)
s=(old/'verify_exports.py').read_text().replace('HansaMill','HansaTowerMill').replace('( .67', '(.67').replace('(.67,.76,.88,1)','(.72,.78,.85,1)').replace("default_value=.7", "default_value=.8").replace('resolution_x=1200','resolution_x=1000').replace('gtao_distance=2','gtao_distance=1').replace('gtao_factor=1.08','gtao_factor=1').replace("['Hero','Base_Detail','Roof_Detail']", "['Front','Base_Detail']")
s=s.replace("asset=[o for o", "# FBX standard material translation needs the same explicit vertex-color multiply as Unreal.\nif fmt=='fbx':\n for ob in bpy.context.scene.objects:\n  if ob.type!='MESH' or not ob.data.vertex_colors:continue\n  for m in ob.data.materials:\n   n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF')\n   if not b or not b.inputs['Base Color'].links:continue\n   source=b.inputs['Base Color'].links[0].from_socket;vc=n.new('ShaderNodeVertexColor');vc.layer_name=ob.data.vertex_colors[0].name;mix=n.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1;l.new(source,mix.inputs[1]);l.new(vc.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])\nasset=[o for o")
(P/'scripts/verify_exports.py').write_text(s)
s=(old/'swatch_density.py').read_text().replace('portable_1024.blend','portable.blend').replace('HansaMill','HansaTowerMill').replace('Large soft daylight','Daylight').replace("['Oak','RoofTimber','Limestone','ForgedIron']", "['Masonry','WeatheredTimber','SagePaint','Fieldstone']")
(P/'scripts/swatch_density.py').write_text(s)
