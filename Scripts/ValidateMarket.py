"""Validate EMVP-P25 native image and semantic evidence without resampling."""
import csv
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Docs/Images/UI/MarketP25/Native"
STATES = "ledger bread sort-focus empty accessible loading error retry relationships world-building".split()


def nodes(path):
    encoding = "utf-16" if path.read_bytes().startswith(b"\xff\xfe") else "utf-8-sig"
    with path.open(encoding=encoding) as source:
        return {row["id"]: row for row in csv.DictReader(source, delimiter="\t")}


def validate():
    for width, height in [(1280, 720), (1920, 1080)]:
        for state in STATES:
            base = EVIDENCE / f"market-{width}x{height}-{state}"
            png = base.with_suffix(".png").read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack(">II", png[16:24]) == (width, height)
            tree = nodes(base.with_suffix(".tsv"))
            if state == "world-building":
                assert tree["CityOverview.Root"]["visible"] == "0"
                assert tree["Inspector.Root"]["visible"] == "1"
                continue
            if state in ("loading", "error"):
                assert not any(n["visible"] == "1" for key, n in tree.items() if key.startswith("Market."))
                if state == "error":
                    assert tree["CityOverview.State.Retry"]["visible"] == "1"
                continue
            assert tree["Market.Root"]["value"] == "10"
            assert tree["Market.List"]["value"] == ("0" if state == "empty" else "10")
            for key in ["Market.Root", "Market.Search", "Market.Header.Good", "Market.Header.Price", "Market.Header.Status"]:
                n = tree[key]
                assert n["visible"] == "1", (state, key)
                x, y, right, bottom = [int(n[k]) for k in ("x", "y", "right", "bottom")]
                assert 0 <= x < right <= width and 0 <= y < bottom <= height, (state, key, (x, y, right, bottom))
            if state in ("bread", "empty", "accessible"):
                assert tree["Market.Detail"]["value"] == "Good.Bread"
                for name in ("Production", "Consumption", "SupplyBalance", "CitizenDemand", "IndustrialDemand", "IncomingSupply"):
                    assert tree[f"Market.Detail.Metric.{name}"]["value"], (state, name)
            if state == "relationships":
                assert any(n["visible"] == "1" and int(n["value"]) > 0 for key, n in tree.items() if key.startswith("Market.Detail.Producer."))
            assert not any(n["visible"] == "1" for key, n in tree.items() if key.startswith("BuildMenu."))
    for path in EVIDENCE.parent.glob("*.png"):
        assert struct.unpack(">II", path.read_bytes()[16:24]) == (1536, 1024), path.name
        assert path.with_suffix(".prompt.md").exists()
    manifest = json.loads((EVIDENCE / "manifest.json").read_text(encoding="utf-8"))
    for name, digest in manifest.items():
        assert hashlib.sha256((EVIDENCE / name).read_bytes()).hexdigest() == digest, name
    print("P25: 20 native captures, semantic bounds, metrics, state recovery, world actions and hashes passed.")


if __name__ == "__main__":
    validate()
