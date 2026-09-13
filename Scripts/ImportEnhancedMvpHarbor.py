"""P17-only staged import using the discovered editor MCP schema; no overwrites."""
import sys,json
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import call
JOB=REPO/'Saved/GenerationJobs/hansa-harbor_P17_20260908'
source=(REPO/'SourceArt/Generated/Buildings/HansaMarket_P15_20260908/scripts/import_market.py').read_text()
source=source.replace('from ue_batch import *','')
source=source.replace('/Game/Hansa/Generated/Staging/Market_P15','/Game/Hansa/Generated/Staging/Harbor_P17')
source=source.replace("assert call(SC,'get_current_level')=='/Game/Hansa/Generated/Staging/Residences_P14/L_Residences_Review'", "assert call(SC,'get_current_level')=='/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP'\nassert not call(AS,'exists',path=ROOT)\nassert not call(AS,'is_dirty',asset_path=call(SC,'get_current_level'))")
source=source.replace("if name=='SM_HansaMarket':rec['simpleCollision']='FBX UCX: floor and four columns; checked by staged test'\n else:", '')
source=source.replace("for axis in ('x','y'):assert rec['bounds']['min'][axis]>=-580 and rec['bounds']['max'][axis]<=580", "for axis in ('x','y'):assert rec['bounds']['min'][axis]>=-400 and rec['bounds']['max'][axis]<=400")
source=source.replace("assert abs(rec['bounds']['min']['z'])<=2", "assert rec['bounds']['min']['z']>=-401 and rec['bounds']['max']['z']<=400")
source=source.replace("('Oak','Clay','Canvas','Stone','Iron','Brass','Wicker','Sack')", "('Oak','WetOak','Iron','Wicker')")
source=source.replace('M_Market_','M_Harbor_').replace('T_Market_','T_Harbor_')
source=source.replace('BP_Market_Review','BP_Harbor_Review').replace('/Script/Hansa.HansaMarketPresentation','/Script/Hansa.HansaHarborPresentation')
start=source.index("roles={'CourtHall'");end=source.index('\ncomponents=',start)
source=source[:start]+"roles={'PierDeck':'SM_HansaDock_Deck4m','QuayEdge':'SM_HansaQuay_Edge4m','QuayCorner':'SM_HansaQuay_Corner','Steps':'SM_HansaPier_Steps','Moorings':'SM_HansaMooring_Post','Hoist':'SM_HansaHarborHoist','Cargo':'SM_HansaHarborTransferSkid'}"+source[end:]
source=source.replace('len(components)==6','len(components)==7').replace('P15_IMPORTED_NOT_PROMOTED','P17_IMPORTED_NOT_PROMOTED')
exec(compile(source,str(__file__),'exec'))
