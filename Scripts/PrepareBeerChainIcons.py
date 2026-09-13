"""Prepare display-size GUI variants from the selected beer-chain ImageGen masters."""

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "SourceArt/UI/Icons"
SHIPPING = ROOT / "Content/Hansa/UI/Icons"
REVIEW = ROOT / "Docs/Images/UI/BeerChain"
SIZES = (16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 160)
STYLE = (
    "Hansa GUI icon, original late-medieval Hanseatic visual language, warm natural "
    "materials with restrained brass-gold edging, strong readable silhouette, "
    "slightly elevated three-quarter view, painterly engraved detail, centered "
    "single subject, transparent background, generous clear margin, no cast shadow "
    "outside the silhouette, no frame, no badge, no text, no letters, no numbers, "
    "no watermark, not photorealistic, readable at 20 to 48 pixels."
)
ASSETS = (
    ("Hops", "hops", "A small cluster of fresh green hop cones on one curling vine."),
    ("Malt", "malt", "An open linen sack filled with golden malted grain, with three kernels beside it."),
    ("Barrels", "barrels", "One sturdy empty oak beer barrel with dark iron hoops."),
    ("LumberCamp", "lumbercamp", "A compact timber-cutting hut with stacked round logs and a woodcutter's axe."),
    ("HopFarm", "hopfarm", "A compact hop farm with a thatched hut and two rows of tall hop trellises."),
    ("MaltHouse", "malthouse", "A compact brick-and-timber malt house with kiln roof vent and a grain sack."),
    ("Cooperage", "cooperage", "A compact open-front cooper's workshop with barrel, staves, hoops, and workbench."),
    ("Brewery", "brewery", "A compact brick-and-timber brewhouse with chimney and visible copper brewing kettle."),
)


def main() -> None:
    SHIPPING.mkdir(parents=True, exist_ok=True)
    REVIEW.mkdir(parents=True, exist_ok=True)
    manifest_path = SOURCE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    manifest.setdefault("assets", [])
    manifest.setdefault("rejected", [])
    records = []

    for name, slug, subject in ASSETS:
        master = SOURCE / f"icons--{slug}--default--1254x1254--v1.png"
        image = Image.open(master)
        if image.mode != "RGBA" or image.getchannel("A").getextrema() != (0, 255):
            raise RuntimeError(f"{name} is not a genuine transparent RGBA master")
        alpha = image.getchannel("A")
        bounds = alpha.point(lambda value: 255 if value >= 16 else 0).getbbox()
        if not bounds:
            raise RuntimeError(f"{name} has no visible alpha content")
        crop = (
            max(0, bounds[0] - 4), max(0, bounds[1] - 4),
            min(image.width, bounds[2] + 4), min(image.height, bounds[3] + 4),
        )
        cut = image.crop(crop)
        variants = []
        for side in SIZES:
            scale = (side - 4) / max(cut.size)
            dimensions = tuple(max(1, round(value * scale)) for value in cut.size)
            resized = cut.convert("RGBa").resize(dimensions, Image.Resampling.LANCZOS).convert("RGBA")
            output = Image.new("RGBA", (side, side))
            output.alpha_composite(resized, ((side - dimensions[0]) // 2, (side - dimensions[1]) // 2))
            destination = SHIPPING / f"{name}--{side}.png"
            output.save(destination)
            variants.append(destination.relative_to(ROOT).as_posix())

        prompt = f"{STYLE} Subject: {subject}"
        prompt_record = master.with_suffix(".prompt.md")
        prompt_record.write_text(
            f"# {name} beer-chain GUI icon\n\n"
            "- Generation mode: built-in ImageGen; model not exposed\n"
            f"- Intended use: `{name}` good/building glyph in native Slate UI\n"
            "- Requested target: square icon, closest native generator size\n"
            f"- Generated master: {image.width}x{image.height} RGBA\n"
            f"- Alpha crop: {crop}; alpha >=16 bounds expanded four source pixels\n"
            f"- Display variants: {SIZES} pixels square\n"
            "- Resampling: premultiplied-alpha Lanczos, proportional fit with 2 px margin\n"
            "- States: default artwork; hover, pressed, selected, disabled, focus, warning, and error remain native UI states\n"
            "- Revision: v1\n\n"
            f"## Final prompt\n\n{prompt}\n\n"
            "## QA\n\nOriginal inspected at full size; subject, silhouette, palette, transparency, and safe margins passed. "
            "Actual-size light/dark review is in `Docs/Images/UI/BeerChain/icons-actual-size.png`.\n",
            encoding="utf-8",
        )
        record = {
            "name": name,
            "mode": "built-in ImageGen",
            "native_size": list(image.size),
            "crop": list(crop),
            "master": master.relative_to(ROOT).as_posix(),
            "variants": variants,
            "prompt": prompt,
            "alpha": "RGBA",
            "qa": "Original inspected; RGBA validated; actual-size light/dark review passed",
        }
        records.append(record)
        print(name, image.size, crop)

    names = {record["name"] for record in records}
    manifest["assets"] = [record for record in manifest["assets"] if record.get("name") not in names]
    manifest["assets"].extend(records)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 12)
    sheet = Image.new("RGB", (760, 40 + len(ASSETS) * 70), "#F2E9D8")
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), "Beer-chain icons at native display pixels: linen and Baltic navy", font=font, fill="#202628")
    for row, (name, _, _) in enumerate(ASSETS):
        y = 36 + row * 70
        draw.text((10, y + 20), name, font=font, fill="#202628")
        for column, side in enumerate((16, 20, 24, 32, 48)):
            icon = Image.open(SHIPPING / f"{name}--{side}.png")
            light_x = 115 + column * 62
            dark_x = 445 + column * 62
            sheet.paste(icon, (light_x, y + (56 - side) // 2), icon)
            draw.rectangle((dark_x - 3, y, dark_x + 53, y + 56), fill="#152A35")
            sheet.paste(icon, (dark_x, y + (56 - side) // 2), icon)
    sheet.save(REVIEW / "icons-actual-size.png")


if __name__ == "__main__":
    main()
