# Construction building inspector — 2026-09-11

The old generic construction inspector now routes to the ordinary compact building
panel. The existing animated ring uses authoritative elapsed/total construction
ticks. Its text and tooltip identify construction rather than production batches.
Completion returns to the normal inspector for the completed building type.

## Reused components and states

- Native compact shell, navy identity header, close control and existing worker portrait.
- Native progress ring, numeric completion, build-time tooltip and construction state.
- Native Details control and scrollable refund/cause information.
- Existing frame/pin/related actions and confirmed cancellation, including X shortcut.
- Existing default, hover, pressed, disabled and keyboard/controller focus states.
- Pause freezes construction animation; reduced motion uses discrete tick progress.
- Production ports, batch records, production pause, cost and labor footer are hidden
  during construction, including their semantic visibility/focus where applicable.

No new raster imagery or ImageGen reference was necessary: this implements reuse
of the approved building design. Existing art source/prompt records are unchanged.
No production art was generated, imported or resized.

## Validation

- HansaEditor Development build passed.
- Hansa.UI.Inspector: 6 tests passed, including compact construction routing,
  elapsed progress, semantic circle label, hidden production pause, focusable
  cancellation, and confirmation before removal.
- Hansa.UI.ProductionInspector: 2 non-rendering tests passed.
- Focused real-viewport capture passed at native 1920x1080 and 1280x720,
  including active construction, focused cancellation, large text/high contrast/
  reduced motion, and return to the completed market inspector.
- Inspected native-size construction crops at both resolutions. Circle is centered,
  percentage is legible and Details/cancellation remain reachable.
- Patch whitespace check passed.

Reproduce native verification:

    Scripts/CaptureProductionInspector.ps1 -Width 1920 -Height 1080 -ConstructionOnly
    Scripts/CaptureProductionInspector.ps1 -Width 1280 -Height 720 -ConstructionOnly

Native captures (evidence, not production assets):
Saved/ProductionInspector/production-{width}x{height}-construction.png
Saved/ProductionInspector/production-{width}x{height}-construction-details.png
Saved/ProductionInspector/production-{width}x{height}-construction-accessible.png
Saved/ProductionInspector/production-{width}x{height}-construction-completed.png

Matching TSV files contain semantic state and native widget bounds.

Final capture logs:
Saved/BuildArtifacts/20260911-073656693-production-inspector-1920-1080/Unreal.log
Saved/BuildArtifacts/20260911-073721675-production-inspector-1280-720/Unreal.log

The broader 720p production capture also flagged its production-only large-text
flow overlapping the fixed pause control. That layout is outside construction
mode and is not claimed fixed here. Construction's focused viewport checks pass.
No shipping package, editor schema, gameplay data or asset promotion was changed.
