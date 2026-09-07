from pathlib import Path
P=Path(__file__).resolve().parents[1];old=P.parent/'hansa-mill_20260906_01/scripts/capture_unreal.py';s=old.read_text();a=s.index('views=');b=s.index('\nloc,tgt=',a)
s=s[:a]+"views={'Front':((0,2400,850),(0,0,850)),'Hero':((1500,2200,1400),(0,0,850)),'Rear':((-1600,-2400,1200),(0,0,800)),'Base':((250,920,430),(0,320,230)),'Roof':((600,950,1400),(0,80,1100)),'Iron':((150,650,1200),(0,270,1090))}"+s[b:]
(P/'scripts/capture_unreal.py').write_text(s)
