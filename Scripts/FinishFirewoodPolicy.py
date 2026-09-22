from pathlib import Path
exec((Path('Scripts/ImplementFirewood.py')).read_text().split('def compiled(t):')[0])
s = Path('Scripts/ImplementFirewoodPolicy.py').read_text()
s=s[s.index('def pipeline(t):'):]
s=s.replace('t=replace(t,marker,code+marker)','t=t.replace(marker,code+marker,1)')
exec(s)
edit('Source/HansaSimulation/Public/Events/HansaDomainEvent.h', lambda t: replace(t,'ResearchCompleted\n','ResearchCompleted,\n\t\tHeatingReserveChanged\n'))
edit('Source/HansaSimulation/Private/Events/HansaDomainEvent.cpp', lambda t: t.replace('case EHansaDomainEventType::ResearchQueued:', 'case EHansaDomainEventType::HeatingReserveChanged: return TEXT("HeatingReserveChanged");\n\t\tcase EHansaDomainEventType::ResearchQueued:',1))
