"""Validate the saved P23 native viewport and semantic evidence."""
import csv
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Docs/Images/UI/HudInspector/Native"
STATES = "default speed production cause actions history residence alert loading error empty accessible localized".split()


def read_nodes(path):
    encoding = "utf-16" if path.read_bytes().startswith(b"\xff\xfe") else "utf-8-sig"
    with path.open(encoding=encoding) as stream:
        return {row["id"]: row for row in csv.DictReader(stream, delimiter="\t")}


def rect(node):
    return tuple(int(node[key]) for key in ("x", "y", "right", "bottom"))


def validate():
    count = 0
    for width, height in [(1280, 720), (1920, 1080)]:
        for state in STATES:
            base = EVIDENCE / f"hud-{width}x{height}-{state}"
            png = base.with_suffix(".png").read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack(">II", png[16:24]) == (width, height)
            nodes = read_nodes(base.with_suffix(".tsv"))
            for key in ["HUD.TopStatus", "HUD.AlertStack", "BuildMenu.Root", "Inspector.Root"]:
                node = nodes[key]
                if node["visible"] != "1":
                    continue
                left, top, right, bottom = rect(node)
                assert 0 <= left < right <= width, (base.name, key, rect(node))
                assert 0 <= top < bottom <= height, (base.name, key, rect(node))
            top_bar = rect(nodes["HUD.TopStatus"])
            for key in ["Pause", "Normal", "Fast", "Fastest"]:
                control = rect(nodes[f"HUD.TopStatus.Speed.{key}"])
                assert top_bar[0] <= control[0] < control[2] <= top_bar[2]
                assert top_bar[1] <= control[1] < control[3] <= top_bar[3]
            if state == "default":
                area = sum((rect(nodes[key])[2] - rect(nodes[key])[0]) *
                           (rect(nodes[key])[3] - rect(nodes[key])[1])
                           for key in ["HUD.TopStatus", "HUD.AlertStack", "BuildMenu.Root"])
                assert area / (width * height) <= .30
                print(f"{width}x{height}: default unobstructed viewport >= {100*(1-area/(width*height)):.1f}%")
            if state in ["loading", "error", "empty"]:
                assert nodes["Inspector.DataStatus"]["value"].endswith(state.title())
                assert nodes["Inspector.Close"]["visible"] == "1"
                assert nodes["Inspector.Action.OpenCause"]["visible"] == "0"
                assert nodes["Inspector.Actions"]["visible"] == "0"
            if state == "production":
                for key in ["Inspector.Identity", "Inspector.Result", "Inspector.Flows", "Inspector.Problem", "Inspector.Actions", "Inspector.History"]:
                    assert nodes[key]["visible"] == "1"
                assert "actual" in nodes["Inspector.Result"]["value"]
            count += 1
    print(f"P23: {count} native captures and semantic layouts passed.")


if __name__ == "__main__":
    validate()
