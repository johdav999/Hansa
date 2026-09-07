# S10-P01 implementation

## Outcome

S10-P01 adds the MVP research progression across the authored-definition, compiler, authoritative simulation, query, native runtime UI, semantic automation, Authoring Studio and test layers.

## MVP catalogue

| Branch | Stable ID | Prerequisite | Deterministic effect |
| --- | --- | --- | --- |
| Commerce | `Technology.Commerce.MarketReports` | Root | Market-report age reduction for `City.Lubeck` |
| Commerce | `Technology.Commerce.TransactionFriction` | Market reports | Transaction-friction reduction for `City.Lubeck` |
| Commerce | `Technology.Commerce.ReserveAutomation` | Transaction friction | Reserve automation for `Route.BalticSea` |
| Production | `Technology.Production.ImprovedMilling` | Root | Throughput for `Recipe.MillFlour` |
| Production | `Technology.Production.ImprovedSawmilling` | Improved milling | Throughput for `Recipe.SawPlanks` |
| Production | `Technology.Production.ImprovedSmithing` | Improved sawmilling | Throughput for `Recipe.SmithTools` |
| Logistics | `Technology.Logistics.WarehouseHandling` | Root | Handling for `Building.Warehouse` |
| Logistics | `Technology.Logistics.CartCapacity` | Warehouse handling | Capacity for `Vehicle.Wagon` |
| Logistics | `Technology.Logistics.RouteScheduling` | Cart capacity | Scheduling for `Route.SaltRoad` |

The nine reviewed Data Assets are stored under `Content/Hansa/Core/Technologies/`. Every effect target is a canonical stable ID; provider IDs, filenames and display text do not participate in gameplay identity.

## Authoritative behavior

- `FHansaQueueResearchCommand` enters the same validated gameplay-command gateway used by player input, AI, RPC and controlled automation.
- One active slot is enforced per house. Cost is charged atomically when queued; all prerequisites must already be complete.
- `ResearchPoliticsAndVictory` advances progress once per fixed tick. Completion applies effects in canonical stable order and publishes `ResearchCompleted`.
- Research state is included in immutable snapshots/projections, deterministic fingerprints and normalized subsystem hashes.
- Snapshot-shaped initialization plus `RestoreCanonical` provides serialization readiness and rejects malformed active progress, unknown completed IDs and duplicate houses.
- Asset Manager scans `/Game/Hansa/Core/Technologies` as runtime `AlwaysCook` content. The Lübeck scenario loads 56 reviewed MVP definitions, initializes one house with 1,000 research points and pins registry hash `DFCFACF59D38D7B1`.

## Graph and editor parity

- The shared graph validator reports missing prerequisite nodes, cycles, unreachable content from the three declared roots, missing roots and invalid stable effect targets.
- `UHansaTechnologyDefinition` is fully reflected with schema, migration, serialization, validation and AI-access metadata.
- The Authoring Studio has a native `Research graph` workspace with bounded Commerce, Production and Logistics lanes plus actionable validation rows. It reads the same technology assets and shared validator as compilation/CI.

## Runtime UI and semantics

- `UHansaResearchPresentationModel` joins compiled definitions to authoritative house research state without exposing mutable containers.
- `SHansaResearchScreen` renders native branch cards, selected details, explicit lock reasons/effects, one-item queue/progress and a normal command-submission intent.
- Stable semantics use `Research.Root`, `Research.Node.*`, `Research.Queue`, `Research.Action.Queue` and `Research.Close`.
- Controller order is deterministic: close, canonical technology order, then queue action. Focus and selection are independent states.

## Visual references

All PNGs in this folder are reference-only and use built-in ImageGen. Shipping surfaces are native Slate; no generated full-screen image or component is imported into `Content/`.

| Reference | Native dimensions | Inspection result |
| --- | --- | --- |
| Runtime composed screen | 1536 × 1024 | Accepted; branch hierarchy, selected detail and queue readable |
| Technology card v2 | 1145 × 1374 | Accepted; transparent ARGB edge, clean focus/selection cues |
| Queue strip | 1974 × 797 / 1975 × 796 | Layout accepted for reference; generated outside backing remained opaque, never production-ready |
| Authoring graph composed screen | 1536 × 1024 | Accepted; lanes and missing/cycle/unreachable diagnostics distinct |
| Authoring graph node | 1536 × 1024 | Reference accepted; opaque outside backing prevents production use |
| Validation row | 1972 × 798 | Reference accepted; opaque outside backing prevents production use |

Sibling `.prompt.md` records contain the final prompt intent, generation mode, dimensions, revision notes and inspection status.

## Verification

- `HansaEditor Win64 Development`: passed.
- `Hansa.Simulation.Research`: 3 passed (graph errors, prerequisites/effects/restore, authoritative command/progress/hash).
- `Hansa.UI.Research`: 1 passed (semantic coverage and controller focus).
- `Hansa.Content.Research`: 1 passed (exact nine-node catalogue and missing-node compiler rejection).
- `Hansa.Architecture.Authoring.EconomicSchemaCoverage`: 1 passed.
- `Hansa.Content.Definitions.EconomicRegistry`: 1 passed.
- `Hansa.UI.RuntimeScenario.PlayableShortageProjection`: 1 passed, including nine cooked technologies and authoritative starting research state.
- `Hansa.Simulation.Kernel.LongRunDeterminism`: 1 passed.
- Repository conventions: S10-P01 adds no finding; the audit remains red only for five pre-existing Sprint 8/9 test names.
- Whole-content validation commandlet: technology assets contribute no issue; it remains red for three pre-existing residence/population-tier linkage issues in the dirty baseline.
