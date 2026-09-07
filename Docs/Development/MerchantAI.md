# S10-P02 merchant AI

## Outcome

The Lübeck shortage scenario now contains one server-side merchant rival. The rival owns house 2, 50,000 Pfennig, 400 research points, one cog, one cargo inventory and one inactive Baltic grain route. It never writes money, inventory, markets, routes, production or research directly.

## Decision contract

`FHansaMerchantAIController` consumes only `FHansaSimulationReadOnlyAccess`, the immutable compiled registry and `FHansaCompiledMerchantAITuning`. On the authored five-tick cadence it considers a bounded set of options:

- activate an owned inactive route when known source supply and destination shortage or price margin clear authored thresholds;
- queue the first affordable, prerequisite-complete preferred technology;
- activate an owned idle production process.

Every chosen action is emitted as the same `FHansaGameplayCommand` used by player input. The header uses `EHansaCommandOrigin::ArtificialIntelligence`; ownership, timing, identity and payload rules are enforced transactionally by `FHansaGameplayCommandGateway`.

Market opportunity evaluation calls `CompareMarketOpportunity`, which is composed from `QueryKnownMarketPrice` and `QueryKnownMarketSupplyDemand`. Unknown reports contain no values and make an option ineligible. The controller never calls unrestricted market-state queries.

Equal utility is resolved from a stable FNV-1a value over campaign seed, decision window and stable option ID, followed by stable-ID order. Equal seed and observations therefore produce identical commands, traces and authoritative state hashes.

## Authored tuning and editor parity

`UHansaMerchantAITuningDefinition` is a normal reflected Hansa definition with complete schema, migration, validation, serialization and AI-access metadata. The generic Authoring Studio details workflow exposes cadence, history capacity, thresholds, utility weights, research priorities and bounded trade plans; no specialized AI editor is added.

The accepted MVP asset is `Content/Hansa/Core/AI/DA_AITuning_MerchantRival.uasset`. It is registered as an `AlwaysCook` primary asset. Registry compilation rejects missing technology, route, vehicle, city and good references and route/vehicle mode mismatches. The reviewed 57-definition runtime registry hash is `31E0950503E56509`.

## Diagnostics

`UHansaRuntimeSimulationHost` exposes a read-only, 32-record decision history. Each entry reports:

- decision tick and ordinal;
- selected goal and reason;
- known report facts with information state and units;
- every considered option, eligibility, utility and seeded tie-break;
- chosen stable option ID;
- submitted command type and authoritative gateway acceptance/error.

This controller history is diagnostic state and does not participate in economy mutation or authoritative checksums.

## Acceptance evidence

- `Hansa.Simulation.AI.RivalRecoversShortageResearchesAndTrades` proves a normal route command, cargo transfer, two completed trade legs and research completion.
- `Hansa.Simulation.AI.SeededCadenceAndTieBreakDeterminism` proves equal-seed trace and checksum equality.
- `Hansa.Simulation.AI.UnknownReportsBlockPrivilegedOpportunity` proves hidden/unknown reports cannot produce a trade command.
- `Hansa.Content.AI.TuningSchemaAndReferences` proves generic editor schema coverage and stable-reference rejection.

AI construction, diplomacy, multiple rivals, credit and unrestricted dynamic route search remain outside the MVP boundary.
