"""Copy native viewport evidence and form diagnostic comparisons without resampling."""
from pathlib import Path
import hashlib
import json
import shutil
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / "Saved/TradeWorkspace/TG14"
out = root / "Docs/Development/TradeWorkspaceProduction/TG-14-captures"
out.mkdir(parents=True, exist_ok=True)
records = []
for path in sorted(source.glob("*")):
    if path.suffix not in (".png", ".tsv"):
        continue
    if path.suffix == ".png" and path.stem[-2:] not in ("02", "04", "06", "08"):
        continue
    dest = out / path.name
    shutil.copy2(path, dest)
    item = {"path": dest.relative_to(root).as_posix(), "sha256": hashlib.sha256(dest.read_bytes()).hexdigest()}
    if path.suffix == ".png":
        with Image.open(dest) as im:
            item["dimensions"] = list(im.size)
    records.append(item)

reference = root / "Docs/Images/UI/TradeWorkspace/trade-workspace--decisions--reference--1536x1024--v1.png"
actual = out / "decisions-1536x1024-scale1.0-04.png"
with Image.open(reference) as ref, Image.open(actual) as game:
    assert ref.size == game.size == (1536, 1024)
    side = Image.new("RGB", (3072, 1024))
    side.paste(ref, (0, 0)); side.paste(game, (1536, 0))
    side.save(out / "reference-left--game-right--3072x1024.png")
    Image.blend(ref.convert("RGB"), game.convert("RGB"), .5).save(out / "reference-game--overlay50--1536x1024.png")

manifest = {
    "date": "2026-09-24",
    "reference": reference.relative_to(root).as_posix(),
    "capture_kind": "Real assembled Development game, explicit saved test campaign; not Shipping or generated imagery",
    "comparisons": "Native-size diagnostic compositions only, no resampling. Layout/state departures documented in TG-14.md.",
    "build": "Saved/BuildArtifacts/20260924-082949085-build-HansaEditor-Win64-Development",
    "tests": [
        "Saved/BuildArtifacts/20260924-082643118-automation-Hansa.UI.TradeMap",
        "Saved/BuildArtifacts/20260924-082745582-automation-Hansa.Integration.TradePresence.CityPrivilegeProjectAndAuthority",
    ],
    "viewport_runs": [
        "Saved/BuildArtifacts/20260924-082644269-gui-repair-1920-1080",
        "Saved/BuildArtifacts/20260924-082742923-gui-repair-1280-720",
        "Saved/BuildArtifacts/20260924-082906685-gui-repair-1536-1024",
        "Saved/BuildArtifacts/20260924-083007098-gui-repair-1280-720",
    ],
    "captures": records,
}
(out.parent / "TG-14-evidence.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(f"Recorded {len(records)} native evidence files.")
