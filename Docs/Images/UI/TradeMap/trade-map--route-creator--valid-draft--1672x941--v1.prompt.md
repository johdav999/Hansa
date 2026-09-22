# Trade map route creator — valid draft

- Status: reference
- Generator mode: built-in ImageGen
- Model: not exposed
- Native size: 1672 × 941
- Transparency: no (24-bit RGB)
- Style anchor: Hansa `design.md` and `Docs/UIDesignBrief.md`; local image attachment failed because of a Windows sandbox read error
- Intended Unreal asset: reference-only; reconstruct with native UMG/Slate
- Revision: v1, establishes the Lübeck–Rostock route-creation direction

## Final prompt

Use case: ui-mockup

Asset type: Hansa high-fidelity composed screen reference, reference-only, never shipped as an interactive bitmap

Primary request: Create an original 1920x1080 landscape GUI reference for planning a simple automated sea trade route between Lübeck and Rostock in the Hansa game.

Screen state: Route creator, valid draft, Lübeck–Rostock route selected.

Composition: Full-screen regional trade workspace for a historical Hanseatic city-builder. One continuous nearly black Baltic-navy top bar. Left column about 300 px wide for route list, one selected Cog, and compact warnings. Large center map about half the screen. Right-aligned 390 px inspector with dark heading and warm linen body for ordered stops and route rules. Bottom-center bounded manifest strip for cargo flow and estimates.

Map: Desaturated ink-and-watercolor geography over dark muted Baltic water, geographically credible southern Baltic coast, readable coastline and rivers, restrained nautical-chart grid. Lübeck and Rostock clearly emphasized with harbor city markers, brass selection rings, and text labels. A single solid Baltic-blue and brass sea-route curve connects them with small directional arrowheads; route thickness suggests one Cog. Hamburg, Wismar and Stralsund appear only as faint future-context labels. Winter delay warning uses an amber clock plus words, never color alone.

Top bar: Generic original Hansa merchant crest, colored coin icon with money, population and workforce icons, centered title "Trade Routes", season and speed controls, close control. Fine aged-brass double frames and small engraved corner ornaments.

Left panel exact text where legible: heading "Routes"; selected item "Lübeck–Rostock"; ship card "Adler — Cog"; state "Draft"; actions "Create route", "Duplicate", "Pause"; one small amber warning tablet with a written cause.

Right inspector exact visible text where legible:
"Route: Lübeck–Rostock"
"Cog: Adler"
"1  Lübeck"
"Load bread — up to 20 t"
"Minimum reserve — 40 t"
"2  Rostock"
"Unload bread — up to 20 t"
"Route valid"
Primary button "Activate route"
Secondary button "Cancel"

Bottom manifest: colored painted bread icon and simple flow Lübeck → Cog → Rostock; labels "Round trip", "Capacity", "Upkeep", "Approx. profit"; use illustrative values sparingly and visibly mark profit as approximate.

Style/medium: polished high-fidelity strategy-game UI reference, historically grounded Hanseatic merchant counting house, modern clarity, restrained tactile materials, crisp native-widget-like layout. Humanist transitional serif headings, highly legible sans-serif body, tabular numerals.

Palette: Baltic Navy #152A35, Harbor Slate #29424D, Ink #202628, Muted Ink #596160, Linen #F2E9D8, Parchment #DFCFAF, Oak #795137, Brass #C19A52, Hanseatic Brick #A44C3F, Prosperity Teal #35766F, Baltic Blue #397FA3, Warning Amber #D09132, Oxblood #762F32, Chalk #FAF7EF.

Materials/textures: almost-black painted navy wood, very subtle linen and rag-paper texture only on light data panels, narrow brass rules, restrained oak accents, inked map lines. Flat backing behind every text block.

Constraints: clear 8 px spacing rhythm; readable hierarchy; map remains visually dominant; controls appear at least 40 px; status uses icon plus text; simple MVP route only. No contracts, convoys, piracy, tariffs, insurance, diplomacy, price-threshold rules, land routes, or additional active cities. Original design. Reference quantities illustrative.

Avoid: copying any existing game's exact layout or assets, fantasy medieval ornament, thick gold borders, excessive parchment, glossy mobile-game panels, modern stock terminal aesthetics, tiny illegible text, nonsensical labels, duplicated controls, watermark, branded logos, photorealistic people, full-screen sepia.

## QA

- [x] Inspected at generated resolution in the ImageGen result
- [x] Correct generated dimensions and 16:9-like aspect recorded
- [x] Palette and visual family match the documented Hansa direction
- [x] No watermark or third-party branding
- [x] Major labels and state hierarchy are readable
- [x] MVP scope is limited to Lübeck, Rostock and one Cog route
- [x] No resampling used
- [ ] Exact cartography and every small baked label require verification during native implementation

