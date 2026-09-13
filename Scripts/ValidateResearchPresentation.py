"""Verify and preserve P27 native-size research captures (no raster resampling)."""
from pathlib import Path
import csv
import hashlib
import json
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "Docs/Images/UI/ResearchP27"
STATES = ("available", "locked", "active", "completed", "market-link", "building-link",
          "accessible-locked", "route-link", "loading-fixture", "error-fixture")


def main():
    (DEST / "Native").mkdir(parents=True, exist_ok=True)
    evidence = []
    for width, height in ((1280, 720), (1920, 1080), (2560, 1440), (3440, 1440)):
        for scale in (80, 100, 140):
            for state in STATES:
                name = f"research-{width}x{height}-scale{scale}-{state}"
                source = ROOT / "Saved/P27" / (name + ".png")
                pixels = source.read_bytes()
                assert pixels[:8] == b"\x89PNG\r\n\x1a\n"
                assert struct.unpack(">II", pixels[16:24]) == (width, height), name
                tsv = source.with_suffix(".tsv")
                rows = {r["id"]: r for r in csv.DictReader(tsv.read_text(encoding="utf-16" if tsv.read_bytes().startswith(bytes([255,254])) else "utf-8-sig").splitlines(), delimiter="\t")}
                if state == "available":
                    action = rows["Research.Action.Queue"]
                    x, y, right, bottom = (int(action[k]) for k in ("x", "y", "right", "bottom"))
                    assert action["enabled"] == "1" and action["visible"] == "1", name
                    assert 0 <= x < right <= width and 0 <= y < bottom <= height and bottom - y >= 47, name
                if state in ("locked", "active", "completed", "accessible-locked"):
                    assert rows["Research.Action.Queue"]["enabled"] == "0", name
                if state == "active":
                    assert "active=Technology.Commerce.MarketReports" in rows["Research.Queue"]["value"], name
                if state == "completed":
                    assert "state=completed" in rows["Research.Node.Technology_Commerce_MarketReports"]["value"], name
                    assert "applied=Reports stay current" in rows["Research.Detail"]["value"], name
                if state == "loading-fixture":
                    assert "loading=1" in rows["Research.Queue"]["value"], name
                if state == "error-fixture":
                    assert "Research could not start" in rows["Research.Queue"]["value"], name
                for item in (source, tsv):
                    shutil.copy2(item, DEST / "Native" / item.name)
                evidence.append(dict(file=f"Native/{source.name}", width=width, height=height,
                                     scale=scale, state=state, sha256=hashlib.sha256(pixels).hexdigest()))
    (DEST / "verification.json").write_text(json.dumps(dict(captures=evidence, native_count=len(evidence),
        profiles=12, primary_action_checks=12, resampled=False), indent=2) + "\n", encoding="utf-8")
    choices = "\n".join(f'<option value="{e["file"]}">{Path(e["file"]).stem}</option>' for e in evidence)
    page = '''<!doctype html><meta charset="utf-8"><title>P27 research comparison</title>
<style>body{font:16px system-ui;margin:24px;background:#f2e9d8;color:#202628}select{font:inherit;padding:8px}section{overflow:auto;border:1px solid #596160;margin:16px 0;max-height:82vh}img{max-width:none;max-height:none;display:block}h2{font-size:20px}</style>
<h1>P27 native research evidence</h1><p>Images are shown at their original pixel dimensions. Scroll to inspect; no raster is resized. Loading and error captures are explicitly injected presentation fixtures; all other states use native controller input and the normal game clock.</p>
<label>Native state <select id="choice">''' + choices + '''</select></label>
<section><img id="native" alt="Selected native research capture"></section>
<h2>Existing approved ImageGen composition — reference only</h2>
<p>Compare bounded three-branch hierarchy, dependency ordering, selected detail, native focus, palette, and persistent queue. The reference's invented technology names, unsupported cancel action, decorative illustrations and wax seals are not gameplay requirements or imported assets.</p>
<section><img src="../Research/research--composed--selected-active--1536x1024--v1.png" alt="Existing research style anchor"></section>
<script>const s=document.querySelector('#choice'),i=document.querySelector('#native');s.onchange=()=>i.src=s.value;s.value='Native/research-1920x1080-scale100-available.png';s.onchange();</script>'''
    (DEST / "comparison.html").write_text(page, encoding="utf-8")
    print(f"P27 verified {len(evidence)} native captures, 12 profiles, 12 visible primary targets; no raster resampling.")


if __name__ == "__main__":
    main()
