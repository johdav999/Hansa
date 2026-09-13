# EMVP-P22 — Construction tray

## Component inventory and state contract (before generation)

Reuse P21's palette, fonts, 8 px spacing grid, native surfaces, action focus and
motion policy. All images are reference-only. Shipping visuals are native Slate;
there are no resized raster thumbnails or baked labels.

| Component | Implementation/reference | States |
| --- | --- | --- |
| Bottom safe-area shell | Existing HUD host, compact persistent category tray | collapsed, expanded |
| Category navigation | P21 native actions; P21 category reference | default, hover, pressed, selected, focus, disabled |
| Bread/Fish/Planks selectors | Labeled native actions with distinct scalable food/material silhouettes; separate new icon references | default, hover, pressed, selected, focus |
| Building card | P21 action/surface language, new detailed card reference | default, hover, pressed, selected, focus, unavailable/locked, dragging, warning, error |
| Chain connectors | P21 native line/arrow between ordered catalog cards | default, blocked |
| Card data | Native name, cost, footprint, workforce/upkeep, input/output and reason | available, unavailable, locked |
| Placement controls | P21 native actions, ordinary P04 intents | selected, disabled, focus, repeat, valid, warning, error |
| Status/tooltips | P21 native labels and shape-redundant feedback | empty, loading, cause/remedy, rejection |
| Decorative imagery | Native glyphs/rules only; existing P21 icon/decor references | non-interactive |
| Charts/overlays | No new chart; existing P04 world ghost/footprint/road overlays | valid, warning, invalid |

The composed reference targets 1536×1024; the card and three icon references target
1024×1024. Returned native dimensions are preserved and recorded. Reference images
are opaque, front-facing, with safe margins; they do not become production assets.

The tray stays below its expanded cards. Selecting Bread reveals Grain Farm → Mill
→ Bakery; Fish reveals Fishery; Planks reveals Lumber Camp → Sawmill. The ordering
and displayed values come from the existing catalog, with no economic rules in UI.
Card drag retains the existing controller bridge and authoritative command path.
Click/Enter/controller Accept selects a card for world placement. Focus and cards
must survive target updates; category/chain changes must discard stale semantics.
Long text wraps with scrolling where needed. Disabled reasons remain readable.
Developer focus IDs and preview serialization remain semantic-only.

## Implementation and evidence

The shipping panel is `SHansaBuildMenu`, hosted at the bottom safe area of
`SHansaRootHud`. Category selection opens the expansion; collapse leaves all five
catalog categories available. Card widgets retain identity during target updates,
which preserves the drag source and keyboard focus. Changing chains removes stale
cards and connectors from semantic resolution. Expansion space yields to placement
feedback; the card region scrolls instead of pushing the category tray offscreen.

Cards use the existing catalog's names, costs, footprint, workforce/upkeep and
input/output quantities. Locked and unavailable cards retain their reasons and
reject activation. Shared native actions supply hover, pressed, selected, disabled,
keyboard/controller focus and warning/error treatment. Bread, Fish, Planks, building
silhouettes and connectors are scalable Slate geometry. No economic definitions,
save schema, provider integration or simulation rules changed.

P04's card drag operation and controller bridge remain intact. Enter on a focused
card uses the actual Slate button path. Selecting a card also retains normal world
placement, rotate, repeat, confirm and cancel intents. Developer focus IDs and raw
preview serialization are no longer painted in the player HUD; automation retains
structured semantics. No screenshot or generated composition is used as a UI layer.

### Selected visual references

All five originals are under `Docs/Images/UI/Construction/`, with a sibling
`.prompt.md` final prompt record for each image:

| File | Native pixels | Built-in ImageGen mode |
| --- | --- | --- |
| `construction--composed--default--1536x1024--v2.png` | 1536×1024 | Edit of generated composition; exactly five supported categories |
| `construction--building-card--default--1254x1254--v1.png` | 1254×1254 | Generate |
| `construction--bread-icon--default--1254x1254--v1.png` | 1254×1254 | Generate |
| `construction--fish-icon--default--1254x1254--v1.png` | 1254×1254 | Generate |
| `construction--planks-icon--default--1254x1254--v1.png` | 1254×1254 | Generate |

The generator returned 1254-square component originals for the requested square
references. These dimensions were preserved. Original-resolution inspection checked
hierarchy, silhouettes, palette, safe margins and consistency with P21. The opaque
card illustration and sample text are visual guidance, not shipping art or economic
values. The native card omits decorative illustration in favor of readable data.
No textures were imported and no raster was resized. P21 remains the reusable
surface/action/state style anchor and supplies the broader state reference set.

### Reproducible validation

Run from the project root with Unreal Engine 5.8:

```powershell
Scripts/RunAutomationTests.ps1 -Configuration DebugGame -TestFilter Hansa.UI.BuildMenu -WithRendering
Scripts/RunAutomationTests.ps1 -Configuration DebugGame -SkipBuild -TestFilter Hansa.UI.Style
Scripts/RunAutomationTests.ps1 -Configuration DebugGame -SkipBuild -TestFilter Hansa.UI.HUD
Scripts/CaptureConstructionTray.ps1 -Width 1280 -Height 720
Scripts/CaptureConstructionTray.ps1 -Width 1920 -Height 1080
python Scripts/ValidateConstructionTray.py
```

The capture harness uses the production Lübeck game map, controller, HUD and tray.
It reads back the real game viewport, with no gallery overlay. It disables desktop
cursor edge panning only inside the isolated capture process and frames the city
through the ordinary camera intent. It exercises native Enter activation and the
actual card drag handler, including cancellation when dropped back over the tray.
The rendered P04 regression separately verifies world deprojection, the placement
ghost and successful drop through the authoritative command gateway.

The long-label capture injects a German expansion string into the existing native
label solely as a layout fixture. It is not a shipped translation. Accessibility
captures use the panel's high-contrast, reduced-motion and large-text preferences.
The taller card content remains scrollable at 720p.

### Scope limits

This is P22 construction-panel evidence. The surrounding city art, legacy root HUD
and inspector polishing belong to their respective prompts. No full Shipping cook
or physical gamepad hardware session was performed for P22. Generated references
remain under Docs, outside production content. Runtime changes depend only on
runtime Slate/model code; editor modules and generation providers are not introduced.


### Acceptance results — 2026-09-08

- DebugGame editor build passed.
- Rendered `Hansa.UI.BuildMenu`: **10 passed**, including the P04 real viewport
  drag/drop journey (`Saved/BuildArtifacts/20260908-213704816-automation-Hansa.UI.BuildMenu`).
- Final non-rendering build-menu regression: **9 passed**
  (`20260908-214238092-automation-Hansa.UI.BuildMenu`).
- Shared `Hansa.UI.Style`: **5 passed** (`20260908-214023588-automation-Hansa.UI.Style`).
- Root `Hansa.UI.HUD`: **4 passed** (`20260908-213236696-automation-Hansa.UI.HUD`).
- Production viewport/native input capture test passed at **1280×720**
  (`20260908-214302639-p22-native-1280-720`) and **1920×1080**
  (`20260908-214332100-p22-native-1920-1080`).
- `ValidateConstructionTray.py`: **16 native captures and semantic layouts passed**.

Accepted PNGs and matching Unreal Unicode TSV snapshots are in
`Docs/Images/UI/Construction/Native/`: compact, Bread, Fish, Planks, locked,
invalid placement, accessible, and long localized-label states at each resolution.
The manifest records native dimensions and SHA-256 hashes. Original-resolution
visual review covered the compact tray, all three chains, locked reasons, invalid
feedback, focus/large text and the compound-word expansion fixture. The category
tray remains visible; connectors are between their ordered cards; taller content
scrolls. No raster resampling occurred.

The verification loop fixed stale widget identity, hidden semantic activation,
feedback-driven vertical overflow, long compound-word clipping, and the collapse
control's width. Engine plugin startup diagnostics occur before test execution;
the listed automation results passed. Full screen-wide P23 polishing is separate.
