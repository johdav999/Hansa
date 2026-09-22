# Residential compound parcels — prompt 2

Implemented 2026-09-14. This is the system and a few functional fixtures; the twelve production compositions and live settlement activation belong to prompt 3.

## Parcel size is authoritative

The existing 8 × 8 m labourer residence is **not** large enough for several full-sized houses. Compounds reserve their complete logical footprint in the existing 4 m grid. The fixtures use **12 × 16 m (3 × 4 cells)** and **16 × 16 m (4 × 4 cells)**, each holding two approved R07 labourer houses at scale `(1,1,1)`. The default compound footprint is 16 × 16 m. Roof overhangs count toward the envelope. The same two-house layout fails validation at 8 × 8 m; shrinking houses fails validation too.

`BuildingDefinition.FootprintWidthCells/HeightCells` must exactly match its compound. Rotation swaps the occupied dimensions. Placement, overlap rejection, click bounds and selection all use the full parcel. Capacity remains one explicit building value (12 in the binding-draft test), not the number of visual houses.

Legacy 8 × 8 m definitions and existing occupied cells are unchanged. `CreateBindingDraft` requires a **new Building stable ID**, copies gameplay values and replaces the footprint in a transient draft for review. It refuses expansion under the existing ID and clears the old upgrade target so an incompatible upgrade chain cannot be inherited accidentally. There is no silent save migration, automatic land acquisition, house scaling or production promotion.

## Runtime contract

- `UHansaResidentialCompoundDefinition` is a primary definition with `Compound.*` identity, schema version 1, explicit footprint and quarter-turn road-front mask, class, stage, district eligibility, weighted layouts, stable slots/variants, promoted mesh/material references, authored transforms, conservative bounds, and entrance/activity graphs. Slot groups cover dwellings, workshops, fences, vegetation and yard props.
- `UHansaBuildingDefinition.ResidentialCompound` is optional. Empty bindings preserve the previous content hash. Bound definitions include the compound content hash and stage/district in compatibility checks. Existing building schema version 5 remains compatible; the save format is unchanged.
- Composition uses a fixed hash of city stable ID plus persistent building value and generation. Layout, optional presence and variant selection use separate stable keys and sorted candidates. Array reordering does not change composition. Shared slot IDs retain variant choices across stages. Position, yaw and unit scale are authored, never randomised to fit.
- The existing logical building retains ownership, population, construction, upgrades, economy, condition, interaction and save identity. Child instances are presentation only. Construction hides the compound together; fallback retains the logical parcel and existing placeholder.
- Placement chooses from allowed quarter turns and requires road contact on the authored +X front after rotation. Authoritative validation also requires an eligible straight/corner/edge context. Logistics and market road-access queries use that same front. The placed world projection selects eligible left/right corner or edge layouts with a straight fallback.
- `QueryCompound()` exposes definition/layout IDs, instance/batch counts, diagnostics and world-space access nodes on the existing projection actor. This is available to Blueprint/automation without adding child gameplay entities.

## Validation and rendering

Validation checks stable/unique keys, stage and weight limits, promoted references, dimensions, exact unit scale, principal alignment within twenty-five degrees of +X (R05 user-requested revision), complete bounds, all possible variant-pair overlaps (except bounded thin fence end-post joints), entry approaches, a single front road node, graph connectivity, and pedestrian clearance around nodes and links. Default radius is 60 cm. Dwelling/workshop approach nodes must be immediately outside their forward envelope. Production authors must still place those nodes against the actual doors during visual review.

Closed structures use conservative full envelopes for access. An explicitly marked open passage may use its authored convex collision below 2.1 m for walking clearance while retaining full geometry bounds for overlap. The passage test validates the approved R08 covered passage; shifting its posts into a turning path correctly fails.

`AHansaCompoundPresentation` has no tick. It groups instances by mesh plus ordered material overrides into HISM components, with no child collision, overlaps, navigation contribution or ticking. Components use the ordinary instance tree, mesh LOD/culling and editor HLOD instancing policy. Rebuilding/reopening removes the previous tagged batches first. Both saved fixtures reopen with exactly one batch each and empty diagnostics.

Required missing mesh/materials or a changed mesh envelope trigger whole-parcel fallback before any partial batch is built. Optional unavailable art is omitted with a diagnostic. The parent remains selectable; individual instances do not create duplicate outline meshes.

## Editor and interchange

The reflected schema is discovered by the existing authoring registry. Nested arrays/structs now export their actual property topology. Validation appears in the existing Studio flow, with compound binding impact information. Mesh/material fields remain unavailable to AI writes; the containing layout array is read-only to AI so nested restrictions cannot be bypassed by replacing it. No provider integration or live provider call was added.

`HansaCompoundAuthoring` provides strict version-1 JSON draft import/export, deterministic validation, impact description and the explicit new-ID binding draft. Native Details on `AHansaCompoundPresentation` expose definition, seed, stage, road context, district, selected layout, diagnostics and access nodes, with **Rebuild Preview**.

Preserved interchange and schema:

- [12 × 16 m fixture JSON](ResidentialCompounds-Prompt2/compound-fixture.json)
- [Compound JSON schema](ResidentialCompounds-Prompt2/Hansa_ResidentialCompoundDefinition.schema.json)

The opt-in `HansaCompoundPreview` commandlet imports the fixture and creates 12 × 16 m and 16 × 16 m test assets plus a preview map. It refuses to overwrite an existing revision. Example after an Editor build:

```powershell
& H:/Unreal/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe ./Hansa.uproject -run=HansaCompoundPreview -Source=Docs/Development/ResidentialCompounds-Prompt2/compound-fixture.json -Revision=R02 -unattended -nop4 -nosplash -NoSound -AllowCommandletRendering
```

Saved Unreal assets:

- `/Game/Hansa/Developer/CompoundPreview/R01/DA_CompoundFixture_12x16`
- `/Game/Hansa/Developer/CompoundPreview/R01/DA_CompoundFixture_16x16`
- `/Game/Hansa/Developer/CompoundPreview/R01/L_CompoundParcels`

These are non-shipping test fixtures under the existing Developer exclusion. No production Core definition, live map or approved house mesh was replaced. The snapshot below is an actual Unreal viewport capture, not generated artwork. The native image and camera metadata are preserved; inspection used a native-pixel crop without resampling. It shows four complete houses, two separate parcels, clear gaps and visible door fronts. No new raster assets, prompts or material variants were generated for this implementation.

![Reopened compound fixtures](ResidentialCompounds-Prompt2/preview-native.png)

## Verification

**46 tests passed** in the final focused and regression runs:

| Suite | Passed |
|---|---:|
| Hansa.Compound | 7 |
| Hansa.Simulation.Placement | 6 |
| Hansa.Simulation.Logistics | 10 |
| Hansa.UI.World | 9 |
| Hansa.Architecture.Authoring.EconomicSchemaCoverage | 1 |
| Hansa.Integration.Save | 13 |

The compound tests cover real-size/bounds/overlap/access validation, deterministic variants and stage stability, all four road rotations and logical occupancy, actual save-envelope reconstruction and compatibility rejection, HISM/fallback behavior, schema/interchange/binding parity, and integration with the production projection/selection/construction path.

Full **HansaEditor Win64 Development** build passed. Full **Hansa Win64 Shipping** build and `VerifyShippingExclusion.ps1` passed, including the executable/receipt scan for editor, automation, worker and provider tokens. Existing Developer/Staging cook exclusions remain in place. `git diff --check -- Source Config` passed. One earlier schema run reported test success but missed normal process completion; the final clean rerun passed through the standard wrapper.

Machine-readable final results are preserved in [verification.json](ResidentialCompounds-Prompt2/verification.json). Full working logs remain under `Saved/CompoundImplementation/`. The commandlet also reported the repository's GameFeatureData asset-manager configuration warning; it saved the map successfully and the map was independently reopened and inspected.

## Deliberate scope limits

- These layouts are test fixtures, not the final historical compositions or a production rollout.
- The placement ghost uses the straight preview layout and a temporary anchor-derived seed before a persistent parcel ID exists; the placed parcel uses its saved identity and actual road context. Corner-only definitions can therefore show the existing safe placeholder during placement. Native editor preview supports all contexts explicitly.
- Access graphs are validated and exposed in world space. This task does not add autonomous pedestrian simulation or replace the existing navigation system.
- HISM/HLOD integration is enabled; a packaged city-scale cook/HLOD build and performance profiling were not run. The Shipping audit proves binary isolation, not a new packaged-content manifest.
- In-place growth of occupied 8 × 8 m parcels is intentionally unsupported. Larger production residences need new definitions and an explicitly authored upgrade/relocation policy.
