"""Prepare alpha-safe Slate density variants from selected built-in ImageGen masters."""
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "SourceArt/UI/TextileProduction"
SHIPPING = ROOT / "Content/Hansa/UI/TextileProduction"
REVIEW = ROOT / "Docs/Images/UI/TextileProduction"
SIZES = (16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 160)
SHIPPING.mkdir(parents=True, exist_ok=True)
REVIEW.mkdir(parents=True, exist_ok=True)

ASSETS = {
    "Flax": ("textileproduction--flax--default--1254x1254--v1.png", "A compact tied bundle of prepared pale-gold flax fibres with a few blue flax flowers as identification, realistically painted Hanseatic city-builder resource icon, three-quarter elevated view, warm upper-left light, transparent alpha, no text, one subject, generous safe margin."),
    "Hemp": ("textileproduction--hemp--default--1254x1254--v1.png", "A compact tied bundle of prepared muted green-brown hemp fibres with a few characteristic leaves as identification, realistically painted Hanseatic city-builder resource icon, three-quarter elevated view, warm upper-left light, transparent alpha, no text, one subject, generous safe margin."),
    "Beeswax": ("textileproduction--beeswax--default--1254x1254--v1.png", "Several irregular golden beeswax cakes and a small honeycomb fragment, realistically painted Hanseatic city-builder resource icon, warm upper-left light, readable on navy and linen, transparent alpha, no text, one subject, generous safe margin."),
    "LinenCloth": ("textileproduction--linen-cloth--default--1254x1254--v1.png", "A neatly folded stack and short roll of undyed warm ivory linen cloth with visible woven texture, realistically painted Hanseatic city-builder resource icon, warm upper-left light, transparent alpha, no text, one subject, generous safe margin."),
    "LinenClothing": ("textileproduction--linen-clothing--default--1254x1254--v1.png", "A folded late-medieval undyed linen tunic with simple lacing and restrained blue trim, realistically painted Hanseatic city-builder resource icon, warm upper-left light, transparent alpha, no mannequin or person, no text, generous safe margin."),
    "Candles": ("textileproduction--candles--default--1254x1254--v1.png", "A tied bundle of slender hand-dipped beeswax candles with two standing candles, warm golden wax, unlit, realistically painted Hanseatic city-builder resource icon, transparent alpha, no holder, no text, generous safe margin."),
    "Rope": ("textileproduction--rope--default--1254x1254--v1.png", "A compact coil of thick hand-laid natural-fibre rope with one short end showing the twist, realistically painted Hanseatic city-builder resource icon, warm upper-left light, transparent alpha, no nautical props, no text, generous safe margin."),
    "Weaver": ("textileproduction--weaver-card--default--1254x1254--v1.png", "A single illustrated late-medieval Hanseatic weaver workshop: modest timber-framed lime-plaster building, broad open work bay showing a wooden loom and restrained yarn bundles, red clay roof, three-quarter elevated construction-card view, transparent alpha, no people, no text, no frame."),
    "Tailor": ("textileproduction--tailor-card--default--1254x1254--v2.png", "A compact late-medieval Hanseatic tailor live-work house: timber frame and lime plaster, red clay roof, window-lit cutting bench and folded cloth stock visible through an open work window, simple hanging shears trade sign, no mannequin, three-quarter elevated construction-card view, transparent alpha, no people, no text, no frame."),
    "Chandler": ("textileproduction--chandler-card--default--1254x1254--v1.png", "A small late-medieval Hanseatic chandler workshop: timber-framed lime-plaster building, red clay roof, sheltered bay with wax pot, wooden candle-dipping rack and restrained finished candle bundles, no modern machinery, three-quarter elevated construction-card view, transparent alpha, no people, no text, no frame."),
    "Ropewalk": ("textileproduction--ropewalk-card--default--1254x1254--v1.png", "A distinctive long narrow late-medieval ropewalk: open-sided covered timber lane with a small head shed, wooden twisting apparatus, prepared fibre bundles and rope coils, red clay roof, three-quarter elevated construction-card view showing its length, transparent alpha, no people, no text, no frame."),
}

records = []
for name, (filename, prompt) in ASSETS.items():
    path = SOURCE / filename
    image = Image.open(path).convert("RGBA")
    alpha_range = image.getchannel("A").getextrema()
    if alpha_range != (0, 255):
        raise RuntimeError(f"{name} does not have genuine transparent alpha: {alpha_range}")
    bbox = image.getchannel("A").point(lambda value: 255 if value >= 16 else 0).getbbox()
    if not bbox:
        raise RuntimeError(f"{name} is empty")
    bbox = (max(0, bbox[0] - 4), max(0, bbox[1] - 4), min(image.width, bbox[2] + 4), min(image.height, bbox[3] + 4))
    crop = image.crop(bbox)
    for side in SIZES:
        ratio = (side * 0.88) / max(crop.size)
        dimensions = tuple(max(1, round(value * ratio)) for value in crop.size)
        art = crop.convert("RGBa").resize(dimensions, Image.Resampling.LANCZOS).convert("RGBA")
        output = Image.new("RGBA", (side, side))
        output.alpha_composite(art, ((side - dimensions[0]) // 2, (side - dimensions[1]) // 2))
        output.save(SHIPPING / f"{name}--{side}.png")
    sha = hashlib.sha256(path.read_bytes()).hexdigest()
    prompt_path = path.with_suffix(".prompt.md")
    prompt_path.write_text(
        f"# {name}\n\n"
        f"- Generation mode: built-in ImageGen generate{' + targeted edit' if name == 'Tailor' else ''}\n"
        f"- Intended use: {'individual workshop construction-card illustration' if name in {'Weaver','Tailor','Chandler','Ropewalk'} else 'individual resource icon'}\n"
        f"- Requested dimensions/aspect: square, closest supported native size\n"
        f"- Generated master: {image.width}x{image.height} RGBA\n"
        f"- Alpha crop: {bbox}; alpha threshold 16 plus four source pixels\n"
        f"- Output variants: {SIZES} pixels square; aspect preserved; premultiplied-alpha Lanczos (GUI exception approved 2026-09-10)\n"
        f"- Revision: {'v2; removed mannequin-like form and emphasized the window-lit workbench' if name == 'Tailor' else 'v1'}\n\n"
        f"## Final prompt\n\n{prompt}\n\n"
        f"## Inspection\n\nNative subject, silhouette, palette, transparency, safe margin, and absence of baked text accepted. "
        f"Actual-size variants reviewed on warm linen and navy surfaces; regenerate if later UI-scale testing reveals lost identity.\n",
        encoding="utf-8",
    )
    records.append({
        "name": name,
        "master": str(path.relative_to(ROOT)),
        "native": list(image.size),
        "alphaRange": list(alpha_range),
        "crop": list(bbox),
        "variants": list(SIZES),
        "method": "transparent-margin crop, square padding, premultiplied-alpha Lanczos proportional resampling, 88% content extent",
        "masterSHA256": sha,
    })

(SOURCE / "display-variants.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 13)
sheet = Image.new("RGB", (1180, 78 + len(records) * 76), "#F2E9D8")
draw = ImageDraw.Draw(sheet)
draw.text((12, 10), "Hansa textile production — actual display pixels on linen and navy", font=font, fill="#202628")
for row, record in enumerate(records):
    y = 48 + row * 76
    draw.text((10, y + 25), record["name"], font=font, fill="#202628")
    for column, side in enumerate((16, 20, 24, 32, 48, 64)):
        art = Image.open(SHIPPING / f"{record['name']}--{side}.png")
        x = 145 + column * 84
        sheet.paste(art, (x, y + (68 - side) // 2), art)
        dark_x = 660 + column * 84
        draw.rectangle((dark_x - 4, y, dark_x + 68, y + 68), fill="#102733")
        sheet.paste(art, (dark_x, y + (68 - side) // 2), art)
sheet.save(REVIEW / "display-size-review.png")
print(json.dumps({"assets": len(records), "variants": len(records) * len(SIZES), "review": str(REVIEW / "display-size-review.png")}))
