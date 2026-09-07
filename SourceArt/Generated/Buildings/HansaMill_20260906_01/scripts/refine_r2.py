from pathlib import Path
p=Path(__file__).with_name('build_mill.py');s=p.read_text()
s=s.replace("random.uniform(-.045,.045)","random.uniform(-.08,.08) if REV>=2 else random.uniform(-.045,.045)")
s=s.replace(".045 if REV>=1 else .06", ".023 if REV>=2 else .045 if REV>=1 else .06")
s=s.replace("rad*math.cos(a),rad*math.sin(a)","(rad+(random.uniform(-.06,.06) if REV>=2 else 0))*math.cos(a),(rad+(random.uniform(-.06,.06) if REV>=2 else 0))*math.sin(a)")
s=s.replace("# Structural fixings", """if REV>=2:
 # Recessed timber liner makes the small front opening legible, avoiding a floating cutout.
 for z in [2.73,3.37]:box('Front_hatch_lintel',(0,-2.13,z),(.85,.18,.095),dark,'Access',.008,0)
 for x in [-.405,.405]:box('Front_hatch_jamb',(x,-2.13,3.05),(.085,.16,.62),dark,'Access')
# Structural fixings""")
p.write_text(s)
