# Production inspector implementation

**Superseded visual layout:** the 2026-09-10 user-requested compact portrait panel is documented in [CompactProductionInspector.md](CompactProductionInspector.md). The data and animation foundation below remains applicable.

Approved anchor: [bakery reference](../Images/UI/Bakery/bakery--panel--baking--1024x1536--v1.png), approved for implementation by the user on 2026-09-09.

## Implemented behavior

The generic native Slate production inspector is used when selecting a completed
production building or inspecting its production projection. Grain farms, mills
and bakeries use the same recipe-driven component. Sources show no required
input; every input and output of a multi-port recipe is retained. Other goods use
the native generic good glyph until their own artwork is supplied.

The panel shows available input (excluding reservations), this unit's reserved
batch inputs, output storage, allocated/required workforce, completed batches,
and produced-so-far. All values come from the existing simulation/registry.
Produced-so-far is completed batches multiplied by the current immutable recipe;
future runtime recipe switching would require historical output accounting.
Missing inventories say Unavailable. Stock can be shared by inventory identity;
it is not falsely attributed to a private building warehouse.

The batch ring paints a brass arc from authoritative ticks plus the host's
fractional clock. Only the ring repaints between simulation ticks. It freezes on
simulation pause, stops for blocked/disabled production and respects reduced
motion. Animation never creates resources or completes a batch. The numeric
percentage and tick count are authoritative discrete values, not productivity.

Pause/resume uses the existing production command. View storage opens the Market
stock overview; Production chain opens the Production overview. Pin, frame,
close and secondary building actions use the existing inspector commands.
Keyboard/controller focus uses real native widgets. The header information
button exposes secondary details, including demolition. Non-production,
construction and unavailable selections retain the established inspector.

## Component inventory and states

| Component | Implementation and states |
| --- | --- |
| Shell/navigation | Native linen/brass border, navy heading, information/close; open/closed |
| Good ports | Source, one or multiple inputs/outputs; engraved SVG for grain/flour/bread |
| Batch overlay | Native circular track/arc and text; running, paused, blocked, reduced motion |
| Recipe band | Native wrapping recipe text and border; source or transformation |
| Stock/record lists | Native label/value/divider; live values or explicitly unavailable |
| Workforce | Native counts and teal bars; full, partial, absent role |
| Status/feedback | Native glyph, heading, cause/remedy; ready, secured, paused, shortage/blocker |
| Controls | Pause/resume, stock/chain, pin/frame, details; default, hover, pressed, selected, disabled, focus |
| Decorations | Separate engraved good illustrations; no portrait or baked dynamic text |
| Loading/error | Existing inspector loading/error presentation remains in use |

## Assets and final prompts

Generation mode: built-in ImageGen, separate new generation per good. Each
requested square 1024 master was returned at **1254 x 1254 native pixels**, accepted
without raster resampling. Every selected source was visually inspected at its
original resolution: recognizable silhouette, clean margins, no labels or clipped
artwork. The composed anchor is native **1024 x 1536**. Its separate reusable
component references and prompts remain under `Docs/Images/UI/Bakery/`.

| Good | Source master and final prompt | Runtime vector |
| --- | --- | --- |
| Grain | [PNG](../../SourceArt/UI/Production/production--grain--default--1254x1254--v1.png), [prompt](../../SourceArt/UI/Production/production--grain--default--1254x1254--v1.prompt.md) | [grain.svg](../../Content/Hansa/UI/Production/grain.svg) |
| Flour | [PNG](../../SourceArt/UI/Production/production--flour--default--1254x1254--v2.png), [prompt](../../SourceArt/UI/Production/production--flour--default--1254x1254--v2.prompt.md) | [flour.svg](../../Content/Hansa/UI/Production/flour.svg) |
| Bread | [PNG](../../SourceArt/UI/Production/production--bread--default--1254x1254--v2.png), [prompt](../../SourceArt/UI/Production/production--bread--default--1254x1254--v2.prompt.md) | [bread.svg](../../Content/Hansa/UI/Production/bread.svg) |

The PNGs are **source/reference art**, not imported production textures. The SVGs
are runtime assets loaded by Slate and explicitly staged as UFS runtime
dependencies in Hansa.Build.cs. They recreate ink as vector geometry from original
samples using `Scripts/TraceProductionGoodIcons.py`; no raster image was resized,
stretched or baked into the interactive panel. The vector brushes are 90 Slate
units and are rasterized natively by Slate. Source hashes and vector provenance:
[provenance.json](../../SourceArt/UI/Production/provenance.json).

## Verification

Development Editor build passed: `Saved/BuildArtifacts/20260909-230357302-build-HansaEditor-Win64-Development`.

- `Hansa.UI.ProductionInspector`: 2 tests passed, including actual inventory joins,
  explicit two-input/two-output fixture, source recipes, missing inventories,
  related-view routes, retained widget identity, fractional pause and real batch
  completion. Log: `20260909-230118630-automation-Hansa.UI.ProductionInspector`.
- Existing `Hansa.UI.Inspector`: 6 tests passed. Log:
  `20260909-225534380-automation-Hansa.UI.Inspector`.
- Real viewport acceptance selects farm, mill and bakery actors; captures moving
  batch samples, native controller pin activation, and combined large-text,
  high-contrast/reduced-motion state. Checks panel bounds, the entire focused
  control, and normal main-action fit at 1080p or larger.
- Native screenshot resolutions: 1280x720, 1920x1080, 2560x1440, 3440x1440.
  Captures are saved at original pixels. TSV files expose bounds and live values.
- Targeted `git diff --check` passed.

Reproduce: `Scripts/Build.ps1 -Target HansaEditor -Configuration Development`,
`Scripts/RunAutomationTests.ps1 -TestFilter Hansa.UI.ProductionInspector -Configuration Development -SkipBuild`,
and `Scripts/CaptureProductionInspector.ps1 -Width 1920 -Height 1080`.

## Scope and remaining limits

This is a native functional reconstruction, **not a verified pixel-identical
copy** of the generated reference. The reference panel occupies about 828x1458
pixels; the established 1080p inspector host is 400x844. Runtime uses project
fonts, responsive spacing, native flat surfaces and scalable recreated artwork.
Small viewports and accessibility text use scrolling rather than resampling.

No gameplay definition or save schema changed. No migration or authoring schema
change is required. The added data structure is an ephemeral presentation join;
existing game/editor definitions remain authoritative. No provider integration
or Editor dependency was added to runtime. A full Shipping cook/package was not
run; staging of the SVGs is declared in the runtime module rules.

The user's open DebugGame editor was not terminated or hot-replaced. Verification
used a separate Development build. Restart/rebuild the DebugGame editor or launch
Development to use this implementation interactively.


Final viewport logs:
- `20260909-230427506-production-inspector-1280-720`
- `20260909-230425691-production-inspector-1920-1080`
- `20260909-230425789-production-inspector-2560-1440`
- `20260909-230425844-production-inspector-3440-1440`

[Live bakery capture](../Images/UI/Production/Native/production-1920x1080-batch-a.png),
[farm](../Images/UI/Production/Native/production-1920x1080-farm.png),
[mill](../Images/UI/Production/Native/production-1920x1080-mill.png),
[large-text focus](../Images/UI/Production/Native/production-1280x720-accessible.png).
All native evidence, hashes and semantic bounds are under `Docs/Images/UI/Production/Native/`.

Final visual inspection at original resolution confirmed the live 1080p bakery
has every main action visible, a legible 22/45 tick label, 48% progress arc, 28
available flour and 2 reserved flour. The 720p combined-accessibility capture
shows the entire focused pause control and a wrapped, fully readable chain
button. Earlier clipped focus and tick/label-wrap defects were corrected before
these final captures. No screenshot pixels were resampled.
