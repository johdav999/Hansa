# S11-P02 — Save/load slots, UI, and round-trip automation

## Runtime slot contract

UHansaSaveSubsystem owns exactly two player-facing slots: manual and autosave. It derives both paths internally beneath Saved/SaveGames/Hansa; no UI, semantic action, MCP request, or gameplay query accepts a path or filename. A save is captured at the authoritative tick boundary and written to a sibling temporary file before an atomic replace.

Slot discovery decodes through the matching UHansaRuntimeSimulationHost without mutating play. Each row reports display name, ISO-8601 UTC timestamp, stable scenario ID, build and format version, simulation tick, authoritative hash, campaign hash, and one of Empty, Compatible, Incompatible, or Corrupt. Compatibility errors preserve the codec category and provide a concrete remedy. Load revalidates the bytes, restores atomically, discards transient caches and diagnostic history, republishes the world projection, and leaves time paused.

## Native UI

UHansaSaveLoadPresentationModel is intent-only. Selecting a populated slot and choosing Save opens overwrite confirmation; choosing Load always opens unsaved-progress confirmation. Empty, incompatible, and corrupt slots disable Load and expose cause plus remedy. Working, success, warning, and error status are represented in the snapshot.

SHansaSaveLoadScreen reconstructs the approved reference using native Slate and shared Hansa style tokens. It contains the screen shell, header, two slot rows, selected-slot metadata panel, Save/Load actions, confirmation layer, and status banner. Dynamic text, timestamps, identifiers, hashes, focus, compatibility, and localized labels are native. No generated raster is shipped or imported.

Stable semantics include:

- HUD.TopStatus.SaveLoad and HUD.SaveLoadHost
- SaveLoad.Root and SaveLoad.Close
- SaveLoad.Slot.manual and SaveLoad.Slot.autosave
- SaveLoad.Action.Save and SaveLoad.Action.Load
- SaveLoad.Confirmation, SaveLoad.Confirmation.Confirm, and SaveLoad.Confirmation.Cancel
- SaveLoad.Status

Slot semantic values include existence, compatibility, timestamp, scenario, format, build, tick, authoritative hash, and remedy. Enabled, selected, focused, warning, error, and visibility states are explicit.

## save_roundtrip_v1

The development-only fixture uses the real UHansaRuntimeSimulationHost. It places construction, activates both player routes, queues research, and advances until projections contain construction, stocked inventories, nonzero market prices, vehicle cargo, research state, merchant-AI activity, and objective progress.

save_create captures a fixed in-memory fixture slot. save_load restores it, compares the authoritative hash and a stable projection digest before continuation, then restores a second independent runtime from the same bytes and advances both exactly one tick. The operation succeeds only when their authoritative fingerprints and projection digests still match. save_assert_roundtrip reports the seven required coverage flags.

MCP exposes only save_list, save_create, save_load, save_wait_for, and save_assert_roundtrip. Valid slot input is the enum manual or autosave.

## Visual references

The component inventory and four generated references are under Docs/Images/UI/SaveLoad. Each PNG is preserved at generator-native resolution with a sibling prompt and inspection record. They are visual references only.

- composed screen: 1536 × 1024
- selected slot row: 1774 × 887
- overwrite confirmation: 1254 × 1254
- incompatible banner: 1672 × 941

## Verification

- Unreal 5.8 no-link compilation validates the production runtime, UHT, HUD, Slate, automation endpoint, and test sources while the main editor holds module DLLs.
- Hansa.Architecture.Automation.Save.RoundTripV1 passes in the isolated validation copy and checks all seven gameplay slices, authoritative hash equality, projection equality, and one-tick deterministic continuation.
- npm test under Tools/HansaMcp passes 24 tests, including fixed-slot save/load/wait/assert flow and rejection of an added filesystem path argument.