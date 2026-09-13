"""User-approved P17 promotion; reuses the fail-closed P12-P15 promotion mechanism."""
import json,sys
from pathlib import Path
import PromoteEnhancedMvpP12P15 as promotion
promotion.EVIDENCE = Path(__file__).resolve().parents[1]/'Saved/GenerationJobs/approved-p17-20260908'
promotion.FAMILIES = [('P17','Harbor_P17','hansa-harbor',[('BP_Harbor_Review','Dock','pierDeck')])]
if __name__=='__main__':
    mode=sys.argv[1]
    if mode=='plan': promotion.plan()
    else:
        families=json.loads((promotion.EVIDENCE/'plan.json').read_text())
        {'assets':promotion.assets,'audit':promotion.audit_assets,'bind':promotion.bind}[mode](families)
