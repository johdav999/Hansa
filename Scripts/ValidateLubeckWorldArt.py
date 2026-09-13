"""Validate P30 native evidence. Technical pass never grants art/release approval."""
import argparse, hashlib, json, shutil, struct
from pathlib import Path

STATES = ("day-25m", "day-65m", "day-120m", "evening-25m", "evening-65m", "evening-120m", "constructed", "restored")

def read_metrics(path):
    raw = path.read_bytes()
    text = raw.decode("utf-16") if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else raw.decode("utf-8-sig")
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path("Saved/P30"))
    parser.add_argument("--output", type=Path, default=Path("Docs/Images/World/LubeckP30"))
    args = parser.parse_args()
    rows = []
    for width, height in ((1280, 720), (1920, 1080)):
        for state in STATES:
            pair = []
            for variant in ("baseline", "candidate"):
                stem = f"{variant}-{width}x{height}-{state}"
                png = args.source / (stem + ".png")
                raw = png.read_bytes()
                assert raw[:8] == b"\x89PNG\r\n\x1a\n", png
                assert struct.unpack(">II", raw[16:24]) == (width, height), png
                metrics = read_metrics(args.source / (stem + ".txt"))
                assert metrics["releaseAccepted"] == "false", stem
                assert int(metrics["frameSamples"]) > 0, stem
                pair.append(metrics)
                rows.append(dict(capture=stem, sha256=hashlib.sha256(raw).hexdigest(), **metrics))
            assert pair[0]["fingerprint"] == pair[1]["fingerprint"], (state, "authority differs")
            assert pair[0]["tick"] == pair[1]["tick"], (state, "tick differs")
    for variant in ("baseline", "candidate"):
        for width, height in ((1280, 720), (1920, 1080)):
            selected = {r["state"]: r for r in rows if r["capture"].startswith(f"{variant}-{width}x{height}-")}
            assert len({selected[s]["fingerprint"] for s in STATES[:6]}) == 1
            assert selected["constructed"]["fingerprint"] == selected["restored"]["fingerprint"]
    args.output.mkdir(parents=True, exist_ok=True)
    for row in rows:
        for suffix in (".png", ".txt", ".tsv", ".geometry.txt"):
            source = args.source / (row["capture"] + suffix)
            shutil.copyfile(source, args.output / source.name)
    report = dict(technicalEvidencePassed=True, releaseAccepted=False, captureCount=len(rows),
        limitations=["Missing Fishery, Warehouse and vegetation production assets", "Brewery Engine cube remains", "Candidate is staged and unapproved", "No GPU/overdraw/package acceptance", "Baseline retains original lighting in evening-labelled states", "Mesh counts exclude Landscape and are not draw-call or camera-frustum counts"], captures=rows)
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Validated {len(rows)} original-size captures and matching authoritative states. Release accepted: false.")

if __name__ == "__main__":
    main()
