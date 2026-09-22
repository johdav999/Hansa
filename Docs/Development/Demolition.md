# Bottom-menu demolition

## Behavior

`BuildMenu.Demolition` is a persistent native toolbar action. It replaces building
placement, focuses the world viewport and changes the pointer to a crosshair.
Clicking a projected building submits the existing authoritative `RemoveBuilding`
command. An unfinished building uses `CancelConstruction` and its existing refund
policy. Success removes the simulation placement through the host and its normal
world projection update; no actor-only deletion is used. The mode remains selected.
Empty ground produces feedback without a command. UI-covered pointer clicks do not
remove world buildings. Escape, right-click, another construction choice, remote
city restrictions and closing the menu cancel the mode.

## Component inventory and states

- Shell/navigation: existing bottom tray and category row, unchanged shared style.
- Control: existing `SHansaAction`, destructive style, generated Minus glyph at 32
  Slate units with existing display-density selection. Default, hover, pressed,
  selected, disabled and independent keyboard/controller focus are shared states.
- Panels/lists: construction cards/chains are hidden during demolition.
- Status: reused native placement feedback surface; active instructions, success,
  empty-target and command rejection text. Error has the existing Error glyph.
- Overlays: crosshair; construction ghost is cleared. No new chart or decoration.
- Semantics: `BuildMenu.Demolition` boolean action and `Demolition.Feedback` text.

This extends behavior using approved components and raster artwork. No new visual
family, generated reference or imported production asset was created.

## Reused artwork

- Master: `SourceArt/UI/Icons/icons--minus--default--1254x1254--v1.png`.
- Prompt: sibling `.prompt.md`; built-in ImageGen, 1254 x 1254 transparent master.
- Shipping variants: `Content/Hansa/UI/Icons/Minus--32.png` and existing densities.
- Existing prompt record documents original inspection and review on navy/linen.
- No image generation or raster modification was performed for this change.

## Compatibility and limitations

The boolean is presentation-only. No gameplay definition, serialized command or
save schema changes, migrations, editor schema changes or provider code are added.
Existing authority checks remain in the simulation gateway.

The existing removal command rejects production/storage and other buildings with
attached dependents, and buildings with cargo obligations. This UI does not add a
cascade-deletion system. Compound residences already supported by the command lose
their residents as a whole parcel. Completed removal has no refund or undo.

## Verification — 2026-09-17

Changed C++ translation units compiled successfully. The running Unreal Editor
locked normal target DLLs, so all five project modules were linked successfully
under `Saved/DemolitionVerification/Binaries/Win64` from the compiled object files.
The active editor has not loaded the new implementation; it needs a rebuild after
closing the editor. Existing unrelated source also reports a first-include order
error in `HansaSimulationPipeline.cpp`.

Added `Hansa.UI.Demolition.ToolbarJourney` regression coverage for semantic
activation, mode switching, empty/missing targets, unfinished/completed removal,
freed-footprint rebuilding, save/load persistence and remote-city restrictions.
Execution stops at existing catalogue validation before reaching these assertions:
loaded hash `BD2FC9656111389A`, expected `0ECFB6BA46CD1344` (106 definitions).
Evidence: `Saved/DemolitionVerification/Tests.log`. The accepted catalogue guard
was preserved. Runtime click testing and multi-resolution visual inspection remain
unverified because initialization fails. Source whitespace checks pass.

Final feedback/semantic refinements also compile. Their final relink encountered concurrent build activity removing the simulation import library; the last successful isolated link predates only those wording/semantic refinements.
