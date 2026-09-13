# Data-driven construction catalog — EMVP-P03

## Runtime contract

The player build menu is materialized from the compiled economic registry. `UHansaBuildingDefinition`
owns construction visibility, category, order, production-chain membership, optional technology
prerequisite, upgrade-only state, and purpose text. Stable `Building.*`, `Good.*`, `Recipe.*`, and
`Technology.*` IDs remain the only identities crossing editor, runtime, and semantic automation.

Cards derive their display name, currency and resource cost, footprint, workforce, and recipe flow
from the same compiled definitions used by simulation. Runtime affordability is queried through
`UHansaRuntimeSimulationHost::QueryConstructionCost`; the UI does not reproduce construction or
inventory formulas. Locked and unavailable states carry a causal text reason and are disabled for
ordinary and semantic activation.

The enhanced MVP exposes five authored categories and thirteen cards. Production is a second-level
selector with these complete chains:

| Output | Ordered construction cards |
| --- | --- |
| `Good.Beer` | `Building.Brewery` (consumes shared `Good.Grain`) |
| `Good.Bread` | `Building.GrainFarm`, `Building.Mill`, `Building.Bakery` |
| `Good.Fish` | `Building.Fishery` |
| `Good.Planks` | `Building.LumberCamp`, `Building.Sawmill` |

`Building.Smithy` remains a valid runtime definition but is explicitly hidden
from the enhanced MVP construction catalog. Adding or revising a visible building is an authored-data
change; `SHansaBuildMenu` contains no per-building card list.

## Editor schema and validation

Building schema version 3 exports all construction-catalog properties with migration, serialization,
reference, validation, and AI-access metadata. Compilation fails closed for invalid local card shape,
missing final-output Goods, missing prerequisite Technologies, incomplete/duplicate chain stages, and
unreachable upgrade-only cards. The global diagnostic codes are `HSA-REGISTRY-036` through
`HSA-REGISTRY-039`; local building diagnostics are `HSA-BUILDING-010` through
`HSA-BUILDING-012`.

The migration is explicit and idempotent:

```powershell
UnrealEditor-Cmd.exe Hansa.uproject -run=HansaEconomicDefinitionSeed -MigrateEnhancedConstructionCatalog -unattended -nop4 -nullrhi
```

It copies only the version-3 construction presentation fields from the canonical MVP seed into the
fourteen existing building assets, refreshes their fingerprints, and saves those exact packages.
Catalog v3 is pinned by `Tests/Golden/economic_catalog_v3.json`; the reload test reconstructs the
pre-migration building shape and proves the catalog-v2 hash.

## Verification

- `Hansa.Content.Definitions.ConstructionCatalogModel` covers deterministic order, missing chain
  members, technology locks/unlocks, missing references, and a hot-reloaded building revision.
- `Hansa.UI.BuildMenu` covers the thirteen-card catalog, Beer/Bread/Fish/Planks expansion, semantic visibility,
  locked selection, authoritative affordability, and placement gateway behavior.
- `Hansa.Architecture.Authoring.EconomicSchemaCoverage` proves the new properties are exported with
  serialization and migration metadata.
- `Hansa.Integration.Authoring.EconomicAssetReload` proves all 72 on-disk definitions match catalog v3
  and that clearing the P03 fields reproduces catalog v2.
