# Trade map active route — sailing

- Status: reference
- Generator mode: built-in ImageGen
- Model: not exposed
- Native size: 1672 × 941
- Transparency: no (24-bit RGB)
- Style anchor: `trade-map--route-creator--valid-draft--1672x941--v1.png` supplied to ImageGen from conversation context
- Intended Unreal asset: reference-only; reconstruct with native UMG/Slate
- Revision: v1, companion monitoring state for the same route

## Final prompt

Use case: ui-mockup

Asset type: Hansa high-fidelity companion screen reference, reference-only

Primary request: Using the immediately preceding generated Hansa trade-route creator as the strict visual style anchor, create a second original 1920x1080 landscape GUI reference showing the same Lübeck–Rostock route after activation, focused on monitoring the live Cog and market effect.

Keep unchanged: exact nearly black Baltic-navy, warm linen, restrained oak, fine aged-brass double-frame, serif heading, legible sans-serif data, painted icon family, corner ornament scale, panel proportions, map rendering style, and crisp spacing from the style anchor.

Screen state: Active route overview; the Cog Adler is currently sailing from Lübeck toward Rostock carrying bread. This is not route editing.

Composition: continuous navy top bar; narrow left route/fleet list; large central southern Baltic ink-and-watercolor map; right linen details inspector; bottom bounded journey timeline and cargo status.

Center map: same geographically credible Lübeck–Rostock coastal region, same two emphasized cities, one route line, a moving Cog marker partway along it. The route line behind the Cog is solid prosperity teal and ahead is Baltic blue; tiny directional arrowheads. A small arrival-time tablet is anchored near the ship. Other cities remain subdued context only.

Left panel exact text where legible:
"Active routes"
"Lübeck–Rostock"
"Adler — Sailing"
"Cargo: Bread 20 t"
one compact amber notice: "Winter delay: +1 day"

Right inspector exact text where legible:
"Adler"
"Cog • Lübeck–Rostock"
"Destination: Rostock"
"Arrival: about 1 day"
"Cargo"
"Bread 20 / 20 t"
"Route status"
"Active"
buttons "Pause route", "Edit route", "Locate ship"

Bottom timeline: Lübeck departure node → Sailing now → Rostock arrival node. Show labels "Loaded 20 t", "In transit", "Expected unload 20 t". Add a restrained effect summary: "Lübeck reserve protected" and "Rostock incoming supply confirmed", each with icon plus text.

Top bar centered title "Trade overview"; preserve money, population/workforce, winter/date, time controls, and close.

Style: polished shippable-quality strategy-game UI reference, historically grounded Hanseatic merchant counting house with modern clarity. Map stays dominant. All dynamic text would be native widgets in production.

Palette: Baltic Navy #152A35, Harbor Slate #29424D, Ink #202628, Muted Ink #596160, Linen #F2E9D8, Parchment #DFCFAF, Oak #795137, Brass #C19A52, Hanseatic Brick #A44C3F, Prosperity Teal #35766F, Baltic Blue #397FA3, Warning Amber #D09132, Oxblood #762F32, Chalk #FAF7EF.

Constraints: Simple MVP monitoring only; communicate status with icon and text; controls visually at least 40 px; generous localization space; no fake modern graphs; no additional active cities or ships.

Avoid: changing the visual system, Anno-like layout copying, contracts, convoys, piracy, tariffs, diplomacy, land routes, price-threshold rules, heavy fantasy ornament, thick gold, glossy mobile styling, illegible microtext, random labels, watermark.

## QA

- [x] Inspected at generated resolution in the ImageGen result
- [x] Correct generated dimensions and 16:9-like aspect recorded
- [x] Matches the route-creator reference as a state change, not a new style
- [x] Ship location, direction, cargo and arrival are visually understandable
- [x] Winter warning uses icon and text
- [x] No watermark or third-party branding
- [x] No resampling used
- [ ] Exact cartography and every small baked label require verification during native implementation

