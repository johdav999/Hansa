from pathlib import Path
exec(Path('Scripts/ImplementFirewood.py').read_text().split('def compiled(t):')[0])
p='Source/HansaEditor/Private/Definitions/HansaFirewoodCommandlet.cpp'
edit(p,lambda t:t.replace("40'000","6'000").replace("36'000","5'400").replace('Recipe->CycleTicks = 60','Recipe->CycleTicks = 100').replace('? 1000 :','? 50 :',1).replace('? 500 :','? 100 :',1).replace('? 1000 : 0','? 200 : 0',1).replace('ConsumptionMilliUnitsPerResidentPerTick = 10','ConsumptionMilliUnitsPerResidentPerTick = 1'))
