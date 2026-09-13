"""Validate P22 native screenshot dimensions and semantic chain geometry."""
import csv
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Docs/Images/UI/Construction/Native"
CHAINS = {
    "bread": ["GrainFarm", "Mill", "Bakery"],
    "fish": ["Fishery"],
    "planks": ["LumberCamp", "Sawmill"],
}

def validate():
    count = 0
    for width, height in [(1280, 720), (1920, 1080)]:
        for state in ["compact", "bread", "fish", "planks", "locked", "placement", "accessible", "localized"]:
            base = EVIDENCE / f"tray-{width}x{height}-{state}"
            png = base.with_suffix(".png").read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack(">II", png[16:24]) == (width, height)
            encoding = "utf-16" if base.with_suffix(".tsv").read_bytes().startswith(b"\xff\xfe") else "utf-8-sig"
            with base.with_suffix(".tsv").open(encoding=encoding) as stream:
                nodes = {row["id"]: row for row in csv.DictReader(stream, delimiter="\t")}
            categories = [row for key, row in nodes.items() if key.startswith("BuildMenu.Category.")]
            assert len(categories) == 5
            for row in categories:
                assert row["visible"] == "1"
                assert 0 <= int(row["x"]) < int(row["right"]) <= width
                assert 0 <= int(row["y"]) < int(row["bottom"]) <= height
            if state == "compact":
                assert not any(key.startswith("BuildMenu.Card.") for key in nodes)
                tray = nodes["BuildMenu.Root"]
                assert int(tray["bottom"]) - int(tray["y"]) <= 96
            if state in CHAINS:
                cards = [nodes[f"BuildMenu.Card.Building_{name}"] for name in CHAINS[state]]
                assert len([key for key in nodes if key.startswith("BuildMenu.Connector.")]) == len(cards) - 1
                for index, card in enumerate(cards):
                    assert card["visible"] == "1"
                    assert int(card["bottom"]) <= int(nodes["BuildMenu.Chains"]["y"])
                    assert all(field in card["value"] for field in ["cost=", "workforce=", "footprint=", "flow="])
                    if index:
                        edge = nodes[f"BuildMenu.Connector.{index-1}"]
                        assert int(cards[index-1]["right"]) <= int(edge["x"]) < int(edge["right"]) <= int(card["x"])
            if state == "locked":
                assert nodes["BuildMenu.Card.Building_Residence_Artisan"]["enabled"] == "0"
            if state == "placement":
                assert nodes["Placement.Validation"]["visible"] == "1"
                assert nodes["Placement.Action.Confirm"]["enabled"] == "0"
            count += 1
    print(f"P22: {count} native captures and semantic layouts passed.")

if __name__ == "__main__":
    validate()
