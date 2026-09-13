# Production product stock popups

Native behavior extension of the approved compact production inspector and batch
popup. No new screen design, raster assets, dimensions, generation mode or prompts.

Component inventory: existing inspector shell, navigation/actions, production
input/output good glyphs and labels, batch ring, stock/details ledger, worker
portrait, and navy/brass popup surface. The changed components are product hit
regions (focusable native borders) and two-line native stock summaries. All other
components reuse their approved treatment.

States: default hidden; mouse hover or keyboard/controller focus opens the popup;
focus draws the existing Ink outline; leaving hover/focus dismisses it. Zero is a
known numeric zero. Missing inventory is explicitly Unavailable. Paused/blocked
production retains live stock inspection. Construction hides product ports.
There is no product press action, selection state, loading animation or new art.

Data comes from read-only inventory/logistics/route projections. Storage totals
unique input/output inventory IDs; market pools are City-owned inventories in the
production city, excluding building/warehouse buffers and other cities. Physical
stock includes reservations. Local jobs contribute CargoQuantity only when either
endpoint is a city market pool. This handles inbound, outbound, market-to-market
and blocked loaded jobs without adding queued requested quantities. Traveling
route vehicles touching this city contribute the selected good's physical cargo
once per cargo inventory. Cancelled/at-stop routes are not travelling. Market
report stock and expected-supply estimates are not added to physical quantities.

Presentation fields participate in equality comparisons to publish stock-only
updates. Tooltip text changes without reconstructing product widgets. Semantic IDs
are Inspector.Production.Input.<GoodId>.Tooltip and Output equivalents, with real
open/closed visibility and the same displayed two-line text. Product IDs participate
in ordinary keyboard/controller focus order and reveal themselves when scrolled.

Validation: Hansa.UI.ProductionInspector.ProductStockAndTransit covers reservations,
multiple pools, city isolation, buffer exclusion, queued pickups, both local
transport directions, deliveries, ships, cancellation, shared buffers and unknown
inventories. RecipeAndInventoryTruth checks native tooltips for every recipe port.
CaptureProductionInspector.ps1 -ProductsOnly captures mouse input/output popups
and the large-text/high-contrast keyboard focus popup in the assembled viewport.


## Verification results — 2026-09-11

- Development Editor build passed: Saved/BuildArtifacts/20260911-133007685-build-HansaEditor-Win64-Development/Build.log.
- ProductStockAndTransit passed: Saved/BuildArtifacts/20260911-132558897-automation-Hansa.UI.ProductionInspector.ProductStockAndTransit/.
- RecipeAndInventoryTruth passed: Saved/BuildArtifacts/20260911-132851125-automation-Hansa.UI.ProductionInspector.RecipeAndInventoryTruth/.
- RealViewport product-only captures passed at native 1920x1080 and 1280x720,
  including input hover, output hover, live update retaining the open widget,
  and keyboard focus with large text/high contrast/reduced motion.
- Actual tooltip PNGs and semantic TSVs are in Saved/ProductionInspector/
  as production-<resolution>-product-{input,output,focus}-tooltip*. Native popup
  images were inspected directly; the assembled inspector was inspected with an
  unscaled screenshot crop. Text is readable with intact borders and margins.
- Existing ClockPauseAndCompletion test fails because its scenario has no working
  batch at the assumed tick. The stock accounting and product widget tests pass;
  this change does not claim the entire production inspector suite is green.
- No new raster assets, prompt set, imports, generation or artwork resizing.
  The result is implemented native UI using approved existing visual components.
