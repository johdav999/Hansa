"""Validate EMVP-P24 native image and semantic evidence without resizing."""
import csv
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Docs/Images/UI/CityOverview/Native"
STATES = "population focus production market rostock-population rostock-production rostock-market loading error accessible localized supply-chain building-cause".split()


def nodes(path):
    encoding = "utf-16" if path.read_bytes().startswith(b"\xff\xfe") else "utf-8-sig"
    with path.open(encoding=encoding) as source:
        return {r["id"]: r for r in csv.DictReader(source, delimiter="\t")}


def validate():
    for width, height in [(1280, 720), (1920, 1080)]:
        for state in STATES:
            base = EVIDENCE / f"city-{width}x{height}-{state}"
            png = base.with_suffix(".png").read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack(">II", png[16:24]) == (width, height)
            tree = nodes(base.with_suffix(".tsv"))
            if state == "building-cause":
                assert tree["CityOverview.Root"]["visible"] == "0"
                assert tree["Inspector.Root"]["visible"] == "1"
                continue
            for key in ["CityOverview.Root", "CityOverview.Close", "CityOverview.City.Lubeck", "CityOverview.City.Rostock", "CityOverview.Tab.Population", "CityOverview.Tab.Production", "CityOverview.Tab.Market"]:
                node = tree[key]
                assert node["visible"] == "1", (state, key)
                x, y, right, bottom = [int(node[k]) for k in ("x", "y", "right", "bottom")]
                assert 0 <= x < right <= width and 0 <= y < bottom <= height, (state, key, (x, y, right, bottom))
            assert not any(n["visible"] == "1" for key, n in tree.items() if key.startswith("BuildMenu.")), state
            if state.startswith("rostock-"):
                assert "unavailable" in tree["CityOverview.Report"]["value"].lower()
                for key, node in tree.items():
                    if key.startswith("CityOverview.Header."):
                        assert "Unavailable" in node["value"], (state, key)
                assert tree["CityOverview.Market.Details"]["visible"] == "0"
            if state in ("loading", "error", "localized"):
                assert not any(n["visible"] == "1" for key, n in tree.items() if key.startswith("CityOverview.Row."))
            if state == "error":
                assert tree["CityOverview.State.Retry"]["visible"] == "1"
    print("P24: 26 native captures, modal bounds, remote knowledge and state visibility passed.")


if __name__ == "__main__":
    validate()
