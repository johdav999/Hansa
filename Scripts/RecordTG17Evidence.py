"""Record TG-17 logs, original artwork provenance and native remote captures."""
from pathlib import Path
import hashlib
import json
import shutil
from PIL import Image

root = Path(__file__).resolve().parents[1]
artifacts = root / "Saved/BuildArtifacts"
out = root / "Docs/Development/TradeWorkspaceProduction/TG-17-captures"
out.mkdir(parents=True, exist_ok=True)

def record(path):
    item = {"path": path.relative_to(root).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    if path.suffix.lower() == ".png":
        with Image.open(path) as im:
            item["dimensions"] = list(im.size)
    return item

def latest(pattern):
    paths = sorted(artifacts.glob(pattern))
    return paths[-1] if paths else None

tests = []
for pattern in ["*-automation-Hansa.UI.TradeMap", "*-automation-Hansa.UI.VisitingTrade",
                "*-automation-Hansa.UI.TradeCreator", "*-automation-Hansa.Multiplayer.Projections",
                "*-automation-Hansa.Multiplayer"]:
    folder = latest(pattern)
    if folder:
        log = (folder / "UnrealEditor.log").read_text(encoding="utf-8", errors="replace")
        tests.append({"directory": folder.relative_to(root).as_posix(),
                      "passed": log.count("Test Completed. Result={Success}"),
                      "failed": log.count("Test Completed. Result={Fail}"),
                      "log": record(folder / "UnrealEditor.log")})

process_runs = []
captures = []
successful_trade_run = None
for folder in sorted(artifacts.glob("20260924-*-two-player-authority")):
    result = folder / "result.json"
    failure = folder / "failure.json"
    if not result.exists() and not failure.exists():
        continue
    process_runs.append({"directory": folder.relative_to(root).as_posix(),
                         "succeeded": result.exists(), "result": record(result if result.exists() else failure)})
    if result.exists() and json.loads(result.read_text(encoding="utf-8"))["operation"] == "TG17RemoteTradeProof":
        successful_trade_run = folder
if successful_trade_run:
    for file in sorted((successful_trade_run / "captures").rglob("*.png")):
        dest = out / (file.parent.name + "--" + file.name)
        shutil.copy2(file, dest)
        captures.append(record(dest))
    for name in ["result.json", "trade-semantics.json"]:
        if (successful_trade_run / name).exists():
            shutil.copy2(successful_trade_run / name, out / name)

masters = []
for master in sorted((root / "SourceArt/UI/TradeWorkspace").glob("*.png")):
    prompt = master.with_suffix(".prompt.md")
    assert prompt.exists(), f"Missing sibling generation provenance: {master}"
    masters.append({"master": record(master), "prompt": record(prompt), "changed_in_TG17": False})

# Diagnostic comparison only: paste both native pixel grids without resizing either source.
reference = root / "Docs/Images/UI/TradeWorkspace/trade-workspace--wide-shell--reference--1672x941--v1.png"
actual = out / "client2-trade-workspace--screenshot-1920x1080.png"
comparison = None
if actual.exists():
    with Image.open(reference) as left, Image.open(actual) as right:
        canvas = Image.new("RGB", (left.width + right.width, max(left.height, right.height)), "#10232d")
        canvas.paste(left, (0, 0))
        canvas.paste(right, (left.width, 0))
        target = out / "wide-reference-left--remote-native-right.png"
        canvas.save(target)
        comparison = {"image": record(target), "reference": record(reference), "actual": record(actual), "resampled": False}

build = latest("*-build-HansaEditor-Win64-Development")
manifest = {"date": "2026-09-24", "prompt": "TG-17", "projection_schema": 13, "intent_schema": 8,
            "build": record(build / "Build.log") if build else None, "tests": tests,
            "process_runs": process_runs, "captures": captures, "comparison": comparison, "existing_generated_masters": masters,
            "new_generated_assets": [], "reference_fidelity_accepted": False, "shipping_certified": False,
            "limitations": "See TG-17.md; failed broader fixtures and inherited visual/packaged gates are not waived."}
(out.parent / "TG-17-evidence.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(f"Recorded {len(tests)} suites, {len(process_runs)} process runs, {len(captures)} native captures and {len(masters)} existing masters.")
