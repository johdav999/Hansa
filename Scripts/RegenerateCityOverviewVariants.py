"""Rebuild the approved proportional GUI variants from retained ImageGen masters.

Run from any directory. Requires Pillow; never invokes a paid generation provider.
The manifest records the deliberate panorama window and each output size.
"""
from pathlib import Path
import hashlib
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "SourceArt/UI/CityOverview/manifest.json"

def main():
    records = json.loads(MANIFEST.read_text(encoding="utf-8"))
    for record in records:
        with Image.open(ROOT / record["master"]) as master:
            image = master.crop(record["crop"]) if record.get("crop") else master.copy()
            for variant in record["variants"]:
                width = variant["width"]
                height = round(image.height * width / image.width)
                output = ROOT / variant["path"]
                image.resize((width, height), Image.Resampling.LANCZOS).save(output)
                variant["height"] = height
                variant["sha256"] = hashlib.sha256(output.read_bytes()).hexdigest()
    MANIFEST.write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f"Rebuilt {sum(len(r['variants']) for r in records)} variants from retained masters.")

if __name__ == "__main__":
    main()
