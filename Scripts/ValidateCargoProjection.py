"""Validate P32 native evidence without changing or resampling images."""
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
STATES = ("loading", "departing", "traveling", "arriving", "berthed", "unloading", "selected", "restored")
SIZES = ((1280, 720), (1920, 1080), (2560, 1440), (3440, 1440))

def metrics(path):
    raw = path.read_bytes()
    encoding = "utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
    return dict(line.split("=", 1) for line in raw.decode(encoding).splitlines() if "=" in line)

def main():
    out = ROOT / "Docs/Images/World/CargoP32"
    rows = []
    for scale, sizes in (("", SIZES), ("Scale14", ((1280, 720),))):
        for width, height in sizes:
            group = {}
            for state in STATES:
                stem = f"cargo-{width}x{height}-{state}"
                image_path = out / scale / (stem + ".png")
                with Image.open(image_path) as image:
                    assert image.size == (width, height), image_path
                    assert image.getchannel("A").getextrema() == (255, 255), image_path
                m = metrics(image_path.with_suffix(".txt"))
                group[state] = m
                rows.append(dict(capture=str(image_path.relative_to(out)).replace("\\", "/"), state=state,
                                 width=width, height=height, uiScale=1.4 if scale else 1,
                                 sha256=hashlib.sha256(image_path.read_bytes()).hexdigest(), **m))
            for key in ("semantic", "vehicle", "route", "inventory"):
                assert len({m[key] for m in group.values()}) == 1, (scale, width, key)
            assert group["loading"]["tick"] == "1" and group["loading"]["cargoMilli"] == "10000"
            assert group["berthed"]["cargoMilli"] == "10000"
            assert float(group["berthed"]["progress"]) == 1.0
            for state in ("unloading", "selected", "restored"):
                assert group[state]["cargoMilli"] == "0" and group[state]["transferMilli"] == "10000"
                assert group[state]["transferTick"] == "12"
            assert len({group[state]["fingerprint"] for state in ("unloading", "selected", "restored")}) == 1
    for state in STATES:
        assert len({r["fingerprint"] for r in rows if r["state"] == state}) == 1, (state, "display changed simulation")
    result = dict(schemaVersion=1, technicalEvidencePassed=True, releaseArtAccepted=False,
                  nativeCaptureCount=len(rows), captures=rows,
                  limitations=["Rostock is the P31 staged candidate", "P11 Fishery role asset remains missing",
                               "Port lanes compress an abstract voyage; actor movement is not authoritative",
                               "Local wagon movement is covered by real logistics projection tests and the P19 native skin review"])
    (out / "validation.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Validated {len(rows)} native captures and matching cargo/inventory identities across all profiles.")

if __name__ == "__main__":
    main()
