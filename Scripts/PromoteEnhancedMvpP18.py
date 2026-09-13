"""Explicitly approved P18 road promotion using the existing fail-closed asset workflow."""
import json,sys
from pathlib import Path
import PromoteEnhancedMvpP12P15 as promotion
promotion.EVIDENCE=Path(__file__).resolve().parents[1]/'Saved/GenerationJobs/approved-p18-20260908'
promotion.FAMILIES=[('P18','Road_P18','hansa-dirt-road',[('BP_Road_Review','Road','straight')])]
if __name__=='__main__':
    mode=sys.argv[1]
    if mode=='plan':promotion.plan()
    else:
        families=json.loads((promotion.EVIDENCE/'plan.json').read_text())
        {'assets':promotion.assets,'audit':promotion.audit_assets,'bind':promotion.bind}[mode](families)
