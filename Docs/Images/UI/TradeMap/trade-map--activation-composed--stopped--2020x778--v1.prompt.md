# Trade Map route activation composed reference

- Status: reference
- Generator mode: built-in ImageGen
- Model: not exposed
- Native size: 2020 × 778
- Transparency: no
- Style anchor: user-provided Trade Map screenshot
- Intended Unreal asset: reference-only; reconstruct with native Slate
- Revision: v1, establishes explicit route state, action, and consequence hierarchy

## Final prompt

Use case: ui-mockup
Asset type: Hansa Trade Map composed screen reference
Input images: Image 1 is the edit target and visual/layout source of truth.
Primary request: Change only the route activation area in the right-side Simple route editor so route state and button outcome are immediately understandable.
Preserve unchanged: the entire screenshot composition, city map, left route list, top bar, editor fields, palette, typography direction, spacing, dark Baltic overlay, linen panel, and all unrelated controls.
Revised activation area: directly above the action button, add a compact native-style status block reading exactly “ROUTE STOPPED” and below it “Ready to depart from Lübeck.” Replace “Start / pause” with one unambiguous primary button reading exactly “Start route”. Beneath it show a quiet helper line reading exactly “Starts immediately and repeats this route.”
State language reference: active-at-stop should use “ROUTE ACTIVE”; travelling should use “IN TRANSIT TO LÜNEBURG · 4 TICKS”; when travelling, the button should read “Pause available at next stop” and look disabled. Error feedback must be amber/oxblood with a warning icon and plain-language cause plus remedy, never only “rejected”.
Style/medium: realistic native Slate game UI mockup, not concept art.
Color palette: Baltic Navy #152A35, Harbor Slate #29424D, Ink #202628, Linen #F2E9D8, Parchment #DFCFAF, Brass #C19A52, Prosperity Teal #35766F, Warning Amber #D09132, Oxblood #762F32.
Constraints: original Hansa counting-house visual language; status is communicated by text and icon as well as color; preserve the screenshot’s wide aspect and hierarchy; no logos; no watermark; no extra controls; no baked production asset intent—reference only.

## QA

- [x] Inspected at original resolution
- [x] Correct dimensions/aspect
- [x] No alpha required for reference
- [x] Palette and style match
- [x] Required reference text is readable
- [x] No unwanted logo/watermark
- [x] State hierarchy and safe margins verified
- [x] No resampling used

