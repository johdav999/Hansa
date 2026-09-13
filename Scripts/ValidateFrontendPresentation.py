"""Verify and preserve P29 native-size frontend captures (no raster resampling)."""
from pathlib import Path
import csv
import hashlib
import json
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "Docs/Images/UI/FrontendP29"
STATES = ("title-empty", "settings", "audio", "controls", "title-return", "credits", "title-new", "new-game", "playing", "pause", "saved", "overwrite", "return-confirmation", "title-continue", "continued", "corrupt-save", "incompatible-save", "accessible-settings", "settings-back", "second-return", "title-load", "load-selection", "load-confirmation", "loaded", "loaded-paused")


def main():
    (DEST / "Native").mkdir(parents=True, exist_ok=True)
    evidence = []
    for width, height in ((1280, 720), (1920, 1080), (2560, 1440), (3440, 1440)):
        for scale in (80, 100, 140):
            for state in STATES:
                name = f"frontend-{width}x{height}-scale{scale}-{state}"
                source = ROOT / "Saved/P29" / (name + ".png")
                pixels = source.read_bytes()
                assert pixels[:8] == b"\x89PNG\r\n\x1a\n"
                assert struct.unpack(">II", pixels[16:24]) == (width, height), name
                tsv = source.with_suffix(".tsv")
                rows = {r["id"]: r for r in csv.DictReader(tsv.read_text(encoding="utf-16" if tsv.read_bytes().startswith(bytes([255,254])) else "utf-8-sig").splitlines(), delimiter="\t")}
                if state in ("title-empty", "title-return", "title-new", "title-continue", "title-load"):
                    action=rows["Frontend.NewGame"]
                    x,y,right,bottom=(int(action[k]) for k in ("x","y","right","bottom"))
                    assert action["visible"]=="1" and action["enabled"]=="1",name
                    assert 0<=x<right<=width and 0<=y<bottom<=height and bottom-y>=47,name
                    assert rows["HUD.TopStatus.Session"]["visible"]=="0",name
                    assert rows["Frontend.Continue"]["enabled"]==("0" if state in ("title-empty","title-return","title-new") else "1"),name
                if state in ("saved", "load-selection"):
                    action=rows["SaveLoad.Action.Load"]
                    x,y,right,bottom=(int(action[k]) for k in ("x","y","right","bottom"))
                    assert action["visible"]=="1" and action["enabled"]=="1",name
                    assert 0<=x<right<=width and 0<=y<bottom<=height and bottom-y>=47,name
                if state in ("corrupt-save", "incompatible-save"):
                    assert rows["SaveLoad.Action.Load"]["enabled"]=="0",name
                    assert ("compatibility=Corrupt" if state=="corrupt-save" else "compatibility=Incompatible") in rows["SaveLoad.Slot.autosave"]["value"],name
                if state in ("overwrite", "load-confirmation"):
                    assert rows["SaveLoad.Confirmation.Confirm"]["visible"]=="1",name
                if state in ("return-confirmation", "second-return"):
                    assert rows["Frontend.Cancel"]["visible"]=="1" and rows["Frontend.Confirm"]["visible"]=="1",name
                for item in (source, tsv):
                    shutil.copy2(item, DEST / "Native" / item.name)
                evidence.append(dict(file=f"Native/{source.name}", width=width, height=height,
                                     scale=scale, state=state, sha256=hashlib.sha256(pixels).hexdigest()))
    (DEST / "verification.json").write_text(json.dumps(dict(captures=evidence, native_count=len(evidence),
        profiles=12, primary_action_checks=84, resampled=False), indent=2) + "\n", encoding="utf-8")
    choices = "\n".join(f'<option value="{e["file"]}">{Path(e["file"]).stem}</option>' for e in evidence)
    page = '''<!doctype html><meta charset="utf-8"><title>P29 frontend comparison</title>
<style>body{font:16px system-ui;margin:24px;background:#f2e9d8;color:#202628}select{font:inherit;padding:8px}section{overflow:auto;border:1px solid #596160;margin:16px 0;max-height:82vh}img{max-width:none;max-height:none;display:block}h2{font-size:20px}</style>
<h1>P29 native frontend evidence</h1><p>Images are shown at their original pixel dimensions. Scroll to inspect; no raster is resized. The flow uses the production title menu, native controller activation, normal settings and isolated test save slots. Corrupt and incompatible states deliberately replace only isolated test files; the native decoder classifies them. The underlying world is the assembled Lübeck game viewport. No raster is resized.</p>
<label>Native state <select id="choice">''' + choices + '''</select></label>
<section><img id="native" alt="Selected native frontend capture"></section>
<h2>Existing approved ImageGen composition — reference only</h2>
<p>Compare the bounded linen menu, restrained navy backdrop, clear primary action, focus and native save/settings/confirmation components. Reference map decoration and illustrative metadata are not imported gameplay assets.</p>
<section><img src="frontend-p29--composed--reference--1536x1024--v1.png" alt="Title composition reference"></section>
<section><img src="frontend-p29--navigation--reference--1536x1024--v1.png" alt="Navigation reference"></section>
<section><img src="frontend-p29--save-slot--reference--1536x1024--v1.png" alt="Save slot reference"></section>
<section><img src="frontend-p29--settings-row--reference--1536x1024--v1.png" alt="Settings reference"></section>
<section><img src="frontend-p29--confirmation--reference--1536x1024--v1.png" alt="Confirmation reference"></section>
<script>const s=document.querySelector('#choice'),i=document.querySelector('#native');s.onchange=()=>i.src=s.value;s.value='Native/frontend-1920x1080-scale100-title-empty.png';s.onchange();</script>'''
    (DEST / "comparison.html").write_text(page, encoding="utf-8")
    print(f"P29 verified {len(evidence)} native captures, 12 profiles, 84 visible primary targets; no raster resampling.")


if __name__ == "__main__":
    main()
