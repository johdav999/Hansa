# Surveyed waterfront opening — 2026-09-12

Normal New Game on the configured Lübeck terrain preview now starts beside the Trave.
This is an inland city waterfront suitable for fishing, not the Baltic coast (outside
the existing survey). No terrain, material or water actor was moved or regenerated.

## Changes

- Camera focus: approximately (-38600, 4600) cm, 20 m inland from a tested fishery
  footprint at grid (-62, 31). Existing saved prototype PlayerStart transforms are
  overridden at runtime on this survey map only.
- Placement and camera bounds now cover the surveyed land: X -201200..201600 cm,
  Y -201600..201200 cm. The outer partial cells are omitted to stay inside the
  4032 m survey. Building cells remain 4 m, with unchanged world/grid origin and
  building scale. There are 1,014,049 cells.
- Water classification is generated from the same retained lake polygons and
  linear river centerlines/widths used to create the map's native Water Bodies.
  Land cells next to water are shoreline cells. Other cells are land; this does
  not introduce new slope/soil suitability rules.
- Cell lookup now uses the existing canonical sorted order with binary search.
- New Game retains its bound world when rebuilding simulation state, so it cannot
  accidentally revert to the small prototype grid.
- Lübeck's normal empty-city opening multiplies its base plank stock by four:
  28 becomes 112. Other goods, other cities, and explicit shortage fixtures are
  unchanged.

The prototype map/headless fixtures retain their original grid and terrain. Runtime
map detection recognizes the configured Lubeck_Terrain_Preview package, including PIE
prefixes. The surveyed map suppresses legacy placeholder topology.

## Data provenance and regeneration

Run `Scripts/GenerateLubeckSurveyPlacement.py` using Python with NumPy. It reads
`SourceArt/Terrain/Lubeck/Survey_20260907/hydrology/water-build.json` and generates
`Source/Hansa/Private/World/HansaLubeckSurveyPlacement.generated.inl`.
The source SHA-256 and OpenStreetMap/ODbL attribution are embedded in the generated
file. It stores 6,530 non-land runs, avoiding a large hand-authored million-cell table.
River widths and channel beds remain the existing inferred modern-data draft, not
measured medieval shoreline. Four-metre cell classification approximates the native
water edges; no new historical or visual approval is claimed.

## Verification

`Hansa.Integration.RuntimeSimulationHost.SurveyWaterfrontOpening` checks:

- survey profile and waterfront camera bounds;
- 112 opening planks;
- normal paid roads, two residences, market and fishery at the founding bank;
- paid road placement near three distant map corners;
- a completed fish batch after 300 simulation ticks and market-served households;
- full-grid save/load with authoritative hash equality.

Initial passing DebugGame run:
`Saved/BuildArtifacts/20260912-122244031-automation-Hansa.Integration.RuntimeSimulationHost.SurveyWaterfrontOpening`.
The 300-tick run took 11.990 seconds in the headless DebugGame test (approximately
40 ms/tick); this is not a rendered-frame benchmark.
All six placement regression tests passed.

The editor connection was unavailable. A fresh rendered shoreline/streaming inspection
has not been performed; native-water source parity and actual simulation are tested,
not a visual acceptance claim.

Restart the editor and select **New Game**. Existing saves preserve their original
placement grids and inventories; they are neither expanded nor replenished silently.
