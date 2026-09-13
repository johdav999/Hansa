# EMVP-P19 — Cog and local cargo vehicle family

Completed and promoted for P19's asset and read-only entity-presentation contract, with Johan's explicit approval on 2026-09-08. Thirty-three packages are saved under `/Game/Mesh/hansa-vehicles`; `Vehicle.Cog` and `Vehicle.Wagon` are bound at authored revision 2. This is not a claim that P32/P34's complete world movement is implemented. [Approval receipt](Evidence/P19ApprovedPromotion-20260908.json) and [source archive](../../SourceArt/Generated/Vehicles/HansaVehicles_P19_20260908/README.md).

## Asset and coordinate contract

Seven editable roles: Cog hull/deck, standing rig, set sail, furled sail, occupied-cargo sacks, wagon body/shafts, shared wheel. Cog +X is bow; origin is waterline, deck approximately +225 cm. Wagon +X is draft direction; origin is ground; wheel axle is Y at +57.5 cm, wheel outer radius 57.4 cm. Separate parts support sail state, cargo presence and absolute wheel rotation without a skeletal vehicle framework.

The hull envelope was informed by the Bremen Cog dimensions 23.27 m by 7.62 m; fittings extend the complete hull mesh to approximately 23.745 m by 7.81 m. Rig height 17.7 m, working platform and wagon dimensions are game reconstruction assumptions, not surveyed medieval measurements. The wagon body is approximately 2.8 m long, with draft shafts extending the full body role to 4.92 m. No horse, driver, harness animation or vehicle physics is claimed.

Runtime `AHansaCargoVehiclePresentation` accepts const trade-vehicle/route snapshots or a const local logistics job. It retains the corresponding stable entity ID, rejects mismatched identity/mode/capacity, clears invalid projections, displays set/furled sail from route lifecycle, and hides cargo before pickup/after delivery. Cargo geometry is an occupied-load cue, never an item count. Absolute wheel distance is cosmetic and idempotent. Collision, overlap, navigation and actor ticking are disabled: simulation remains authoritative.

`UHansaVehicleDefinition.PresentationActorClass` is an optional, compatible schema-1 extension with authoring metadata, typed class restriction, validation and hash participation when bound. Empty legacy definitions retain their previous canonical hash. Staging/developer classes cannot be loaded as production presentations. Catalog v8 is `6FAA28CD24E2C69E`, previous v7 `22248A11101B32B0`. Only the two vehicle presentation hashes changed; capacities, upkeep, IDs and route rules did not. The seeder, golden catalog, runtime pin, editor schema, visible manifest and backward catalog reconstruction were updated together. Existing v7 saves require a new game; no silent migration is claimed.

## Research and generation

- [Deutsches Schifffahrtsmuseum, Bremen Cog](https://www.dsm.museum/en/museum/exhibits/bremen-cog) and its [museum booklet](https://urn.dsm.museum/Booklet-01e-Cog.pdf): hull construction and archaeological limitations. Local booklet pages were rendered and inspected; no museum pixels are used in shipping textures.
- [Cultural Heritage Agency of the Netherlands, Bremen Cog](https://mass.cultureelerfgoed.nl/bremen-cog-1380): carvel bottom, clinker sides and reconstruction context. Rig details are interpretive, not surviving original rigging.
- [National Museum in Szczecin, MNS/A/19654](https://inmuseums.pl/all-objects/DJXKZ8E3Ai2bOQa411XK_replica-of-spoked-wheel-): comparative hub/spoke/felloe construction. This is an earlier-period 78 cm wheel, not evidence for the game's larger wagon dimensions.
- [Langdon's 1983 thesis](https://etheses.bham.ac.uk/id/eprint/14029/1/Langdon1983PhD.pdf), printed pages 17-18: comparative collar, traces, cart-saddle and double-shaft arrangements. PDF page 34 was rendered and inspected, including the Luttrell-derived cart illustration. This is comparative English evidence, not a Lübeck wagon survey or a claim of direct manuscript inspection.

HansaModels is the authoring workflow. Built-in ImageGen produced one new opaque linen color input at native 1254 x 1254. Existing approved oak/canvas inputs were copied unchanged. Six hybrid materials use independent roughness and tangent-normal construction: oak, tarred oak, linen, hemp, iron and cargo canvas. Eighteen portable maps are native 1024 x 1024 shader bakes, not resized raster source artwork. The linen prompt and source remain together under the P19 job's `textures` folder.

## Current evidence

Job root: `Saved/GenerationJobs/hansa-vehicles_P19_20260908`.

- Four Blender revisions, with original-size render/inspect/correct cycles. Corrections include hull strake gaps, clipped deck planks, stair stringers, mast UV grain, cargo height, wheel-rim topology and UVs, mast partners and windlass.
- Packed `exports/HansaVehicles.blend`, seven FBX and seven GLB role exports. Clean reimports verify bounds within 2 mm and UV presence; separate native renders retained.
- Staged assets under `/Game/Hansa/Generated/Staging/Vehicles_P19`: seven meshes, six materials, eighteen textures and two Blueprints. Three conventional LODs per mesh; Nanite disabled. Mesh simple collision exists for asset inspection, while runtime components never affect physics/navigation.
- DebugGame editor build passes. Read-only contract test passes. The second real-simulation review run passes both tests without warnings: actual trade entity plus a job dispatched/picked up by the deterministic inventory/road logistics pipeline, not a fabricated projection.
- First review failed and is retained: the saved harbor berth had zero yaw, and the default trade scenario supplied no local job. The corrected isolated review explicitly uses yaw 90 and 1220 cm berth offset, and a real logistics fixture.

## Final verification and limits

Twenty-seven targeted tests pass without warnings: production native berth/set-sail/120 m/wagon views, schema metadata, catalog reload with v7 reconstruction, save/load and harbor contracts. DebugGame and Development editor builds pass. Development initially hit MSVC C1001 in an existing test unity unit and succeeded on retry without a source workaround. Initial catalog commandlet reported unrelated GameFeatures configuration errors; the read-only rerun with process-only authoring-plugin exclusions passed.

Shipping binary exclusion, production hard/soft dependency audit and expanded cooked-package audit pass. All 33 vehicle packages are present in the cook; no staging/editor/worker/provider artifacts were found in the audited runtime files. These checks do not claim a final IoStore-container or whole-game release audit. Native Unreal captures are 1280 x 720 and were inspected after save/reopen and after promotion. Six repeated material swatches and clean FBX/GLB renders were also inspected. [Material assessment and native-size comparisons](../../SourceArt/Generated/Vehicles/HansaVehicles_P19_20260908/MATERIAL_ASSESSMENT.md) retain corrections and uncertainties.

The harbor's construction now restores yaw 90 and X=1220 cm, leaving approximately 29.5 cm clearance from the complete hull beam to the pier end; its deck/water elevations are unchanged. This is a native presentation amendment, not a rewrite of the P17 source receipt or a new shoreline survey.

No horse, driver, animated harness, vehicle physics, continuously moving two-city actor manager or new simulation authority. P19 proves production skins can represent actual trade entities and dispatched local jobs, not ambient decoration. The default trade scenario currently has no local jobs, so local cargo acceptance uses a deterministic road/inventory fixture. P32/P34 own full world-projection lifecycle and complete route/delivery journeys. P16/P17 and unrelated visible-content statuses remain untouched by this acceptance.
