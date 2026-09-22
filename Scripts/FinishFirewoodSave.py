from pathlib import Path
exec(Path('Scripts/ImplementFirewood.py').read_text().split('def compiled(t):')[0])
edit('Source/HansaSimulation/Private/Save/HansaSaveEnvelope.cpp',lambda t:replace(t,'HANSA_SAVE_COMMAND(QueueResearch);','HANSA_SAVE_COMMAND(QueueResearch); HANSA_SAVE_COMMAND(SetHeatingReserve);'))
edit('Source/HansaSimulation/Private/Save/HansaSaveFields.inl',lambda t:t+'\nvoid Value(FHansaSetHeatingReserveCommand& V) { Value(V.MarketBuildingId); Value(V.ReserveDays); Value(V.bReleaseProtection); }\n')
