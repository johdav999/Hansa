from pathlib import Path
p=Path(__file__).with_name('turntable_final.py');s=p.read_text().replace('import bpy,pathlib,math,sys','import bpy,pathlib,math,sys,json,itertools')
s=s.replace("cam.data.lens=38", "cam.data.type='ORTHO';cam.data.ortho_scale=16.5;framing=[];bounds=json.loads((P/'exports/geometry.json').read_text())['bounds_m']")
s=s.replace("S.render.filepath=str(P/'renders/turntable'/f'{i:03}.png');", """bpy.context.view_layer.update();inv=cam.matrix_world.inverted();pts=[inv@Vector(v) for v in itertools.product(*[(bounds[0][j],bounds[1][j]) for j in range(3)])];screen=[(.5+v.x/16.5,.5+v.y/16.5) for v in pts];low=min(min(v) for v in screen);high=max(max(v) for v in screen);assert low>.03 and high<.97,(i,low,high);framing.append({'frame':i,'minimum_normalized':low,'maximum_normalized':high});S.render.filepath=str(P/'renders/turntable'/f'{i:03}.png');""")
s=s.replace("print('TURNTABLE_SAFE_FRAMING',flush=True)","(P/'exports/turntable_framing.json').write_text(json.dumps(framing,indent=2));print('TURNTABLE_SAFE_FRAMING',flush=True)")
p.write_text(s)
