"""Verify and preserve P28 native-size session captures (no raster resampling)."""
from pathlib import Path
import csv
import hashlib
import json
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "Docs/Images/UI/SessionP28"
STATES = ("opening", "camera", "pause", "progress", "saved", "load-confirmation", "restored-paused", "construction", "roads", "inspection", "market", "routes", "failure-fixture", "victory-fixture")


def main():
    (DEST / "Native").mkdir(parents=True, exist_ok=True)
    evidence = []
    for width, height in ((1280, 720), (1920, 1080), (2560, 1440), (3440, 1440)):
        for scale in (80, 100, 140):
            for state in STATES:
                name = f"session-{width}x{height}-scale{scale}-{state}"
                source = ROOT / "Saved/P28" / (name + ".png")
                pixels = source.read_bytes()
                assert pixels[:8] == b"\x89PNG\r\n\x1a\n"
                assert struct.unpack(">II", pixels[16:24]) == (width, height), name
                tsv = source.with_suffix(".tsv")
                rows = {r["id"]: r for r in csv.DictReader(tsv.read_text(encoding="utf-16" if tsv.read_bytes().startswith(bytes([255,254])) else "utf-8-sig").splitlines(), delimiter="\t")}
                if state in ("opening", "pause", "restored-paused"):
                    action = rows["Scenario.Begin" if state == "opening" else "Scenario.Resume"]
                    x, y, right, bottom = (int(action[k]) for k in ("x", "y", "right", "bottom"))
                    assert action["enabled"] == "1" and action["visible"] == "1", name
                    assert 0 <= x < right <= width and 0 <= y < bottom <= height and bottom-y >= 47, name
                    assert rows["HUD.TopStatus.Session"]["visible"] == "0", name
                if state in ("camera", "construction", "roads", "inspection", "market", "routes"):
                    for key in ("Session.Help.Dismiss", "Session.Help.Hide"):
                        action=rows[key]
                        assert action["visible"] == "1", name
                        x,y,right,bottom=(int(action[k]) for k in ("x","y","right","bottom"))
                        assert 0 <= x < right <= width and 0 <= y < bottom <= height and bottom-y >= 47, name
                if state == "saved": assert rows["SaveLoad.Status"]["visible"] == "1", name
                if state == "load-confirmation": assert rows["SaveLoad.Confirmation.Confirm"]["visible"] == "1", name
                if state == "failure-fixture": assert "insolvency" in rows["Scenario.Outcome"]["value"], name
                for item in (source, tsv):
                    shutil.copy2(item, DEST / "Native" / item.name)
                evidence.append(dict(file=f"Native/{source.name}", width=width, height=height,
                                     scale=scale, state=state, sha256=hashlib.sha256(pixels).hexdigest()))
    (DEST / "verification.json").write_text(json.dumps(dict(captures=evidence, native_count=len(evidence),
        profiles=12, primary_action_checks=36, resampled=False), indent=2) + "\n", encoding="utf-8")
    choices = "\n".join(f'<option value="{e["file"]}">{Path(e["file"]).stem}</option>' for e in evidence)
    page = '''<!doctype html><meta charset="utf-8"><title>P28 session comparison</title>
<style>body{font:16px system-ui;margin:24px;background:#f2e9d8;color:#202628}select{font:inherit;padding:8px}section{overflow:auto;border:1px solid #596160;margin:16px 0;max-height:82vh}img{max-width:none;max-height:none;display:block}h2{font-size:20px}</style>
<h1>P28 native session evidence</h1><p>Images are shown at their original pixel dimensions. Scroll to inspect; no raster is resized. Victory/failure and individual topic presentations are explicitly injected fixtures. Opening, pause, progress and save/load use native controller input, isolated save slots and the normal game clock.</p>
<label>Native state <select id="choice">''' + choices + '''</select></label>
<section><img id="native" alt="Selected native session capture"></section>
<h2>Existing approved ImageGen composition — reference only</h2>
<p>Compare the linen dossier, navy header, clear primary action, restrained brass borders and optional compact help. Invented logos, painted lettering and scenery in the references are not shipping assets. All game text and controls are native Slate.</p>
<section><img src="session-p28--composed--paused--1536x1024--v1.png" alt="Session composition reference"></section>
<section><img src="session-p28--pause-menu--focus--1024x1536--v1.png" alt="Pause component reference"></section>
<section><img src="session-p28--context-coach--focus--1536x1024--v1.png" alt="Context coach reference"></section>
<script>const s=document.querySelector('#choice'),i=document.querySelector('#native');s.onchange=()=>i.src=s.value;s.value='Native/session-1920x1080-scale100-opening.png';s.onchange();</script>'''
    (DEST / "comparison.html").write_text(page, encoding="utf-8")
    print(f"P28 verified {len(evidence)} native captures, 12 profiles, 36 visible primary targets; no raster resampling.")


if __name__ == "__main__":
    main()
