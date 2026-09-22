# Single labour-house construction button — 2026-09-15

## Behavior
The construction tray exposes one ordinary labour-house button. Approved compound
families remain in the internal catalog for random selection, placement previews,
cost validation and explicit developer/test placement. Each successful continuous
placement chooses the next family from the existing shuffled pool. Invalid
placement and rotation retain the current preview. The ordinary button remains
selected and receives focus after cancellation or drag completion.

## Components
Reused native construction tray, 48x48 building button, tooltip, shared
hover/pressed/selected/disabled/focus states and valid/warning/error placement
feedback. No new artwork, assets, dimensions, prompts or generation pass.
Family buttons are excluded from rendering, controller focus and semantic actions.
No gameplay schema, stable ID, content asset or save-format change.

## Verification
- Unreal 5.8 Development Editor compilation succeeded in the existing isolated
  verification project.
- Added regression coverage to Hansa.Compound.RandomLabourConstruction for one
  semantic labour button, no rendered/activatable family buttons, selected card
  mapping and cancelled-drag return focus.
- Attempted random construction and Hansa.UI.BuildMenu automation. Fixture startup
  was blocked by existing cooked economic registry hash mismatch:
  actual 269ADF8401780434, reviewed catalog v19 31FB425080110FD0.
- Log: Saved/Logs/SingleLabourButtonTests.log.
- Live viewport QA and loading the new binary into the already running editor
  remain unverified. The running editor was left open.