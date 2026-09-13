# Survey placement input — UAT-001

Product: Hansa Unreal game, local surveyed Lübeck opening, player role.
Scope: waterfront construction targeting, remote land targeting, boundary rejection.
Run: `Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Integration.RuntimeSimulationHost.SurveyWaterfrontOpening -Configuration Development`.
No artwork, UI styling or gameplay balance changes.

## Issue ledger

| ID | Severity | Flow | Expected / observed | Regression | Status |
| --- | --- | --- | --- | --- | --- |
| UAT-001 | P1 | Select building and point at waterfront land | Valid surveyed land should reach construction validation; pointer targeting instead rejected every negative-X waterfront cell | SurveyWaterfrontOpening exercises WorldToPlacementCell at the start, bank and four corners, then uses converted targets for paid construction | Verified with automated input-boundary substitute; running DebugGame deployment pending editor closure |

## Root cause and baseline

The simulation was expanded to survey coordinates X -473..533, Y -484..522,
but `AHansaLubeckWorldFoundation::WorldToPlacementCell` retained prototype bounds
X 0..59, Y 0..39. The new start resolves to approximately (-67,31).
`ResolvePlacementCellAtScreenPosition` returned false before a build command could
be created, affecting click, drag and road targeting.

Earlier acceptance used direct grid commands and missed this player-input gate.
The new regression reproduced six failed pointer assertions before the fix.
Baseline:
`Saved/BuildArtifacts/20260912-125743869-automation-Hansa.Integration.RuntimeSimulationHost.SurveyWaterfrontOpening/UnrealEditor.log`.

## Fix and acceptance

Use the existing survey bounds for survey worlds; preserve the prototype branch.
Check the starting location, fishery anchor, four extreme cells, coordinate round
trips and rejection beyond the map. Construct the test chain using targets converted
through the same world-to-cell gate used by the player controller; retain fishing,
distant construction, water/occupancy rejection and save/load checks.

The test runs the production input boundary in an Unreal world. It is an explicit
automated substitute for a live rendered mouse/click replay, not a claim that every
viewport, terrain raycast or pointer gesture has been visually verified.

Development verification passed:

- SurveyWaterfrontOpening, including pointer-derived construction and save/load:
  `Saved/BuildArtifacts/20260912-125902086-automation-Hansa.Integration.RuntimeSimulationHost.SurveyWaterfrontOpening`.
- Both original Lübeck content/map tests:
  `Saved/BuildArtifacts/20260912-130000388-automation-Hansa.Content.World.Lubeck`.
- Three additional Lübeck world tests:
  `Saved/BuildArtifacts/20260912-125933945-automation-Hansa.World.Lubeck`.

DebugGame build was blocked by the running UnrealEditor-Win64-DebugGame process
locking its DLL. No editor process was terminated and no live save was changed.
Close that editor before rebuilding DebugGame; restarting the old binary alone does
not install the source fix. No new game is required solely for this input-boundary fix
when the save already contains the expanded surveyed grid.
