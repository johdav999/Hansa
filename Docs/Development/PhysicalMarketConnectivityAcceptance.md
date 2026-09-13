# Physical market connectivity — prompt 5 acceptance

## Outcome

The physical-market road economy is accepted at source, authored-content, automation, real-viewport, Development-target, and Shipping-target levels. Prompt 5 found and fixed one feature defect: `Building.Fishery` was shoreline-gated but did not require a road, so placement and proactive diagnostics differed from the other production chains. Building schema v5 now requires every recipe-bearing production building to set `bRequiresRoad`; Fishery requires both shoreline and road access.

The reviewed economic catalog is v10 with registry hash `547A8E4FB77941CD`. The supported asset migration updates all 14 building definitions to schema v5 and Fishery to authored revision 2. Save compatibility is explicit for reviewed catalogs v9, v8, and v7; unknown definition hashes still fail closed.

Two release-gate defects outside the road algorithm were also fixed: the generic foundation schema golden was refreshed for its already-authored `PresentationMesh` property, and `HansaTests` now declares its Landscape dependency so the non-editor Development target compiles its Rostock capture test.

## Implemented rules

- A completed operational authored market is the only local-economy access provider.
- Four-way completed roads must continuously connect each physical endpoint to an eligible market in the same city and ownership domain.
- Every bread, fish, planks, and beer production building uses the same road-to-market policy. Fishery additionally requires shoreline placement.
- Disconnected buildings may consume stored inputs and retain local output, but cannot receive or deliver external goods.
- Warehouses, residences, and local harbor handoffs cannot bypass market connectivity. Separate market road components cannot exchange through the city reporting aggregate.
- Jobs revalidate topology before pickup and delivery. Reservations, cargo, completed travel, alternate routes, paused fleet capacity, reconnection, and save/load are deterministic and conserved.
- The inspector and typed automation endpoints read the same authoritative access, route, stock, reservation, and cargo state.

## Acceptance matrix

| # | Scenario | Automated proof |
|---|---|---|
| 1 | Producer beside an isolated road cannot exchange | `Hansa.Simulation.Logistics.PhysicalMarketAccessAndDeterministicSelection`; `DisconnectedRoadAndFullDestinationBottlenecks` |
| 2 | Continuous road to a completed market enables delivery | `CapacityPickupDelayAndCompletedDelivery`; `PhysicalMarketConnectivityJourney` |
| 3 | Bread, fish, and planks share the rule | `Hansa.Integration.PhysicalMarketConnectivity.ProductionFamiliesSharePolicy`; schema validation `HSA-BUILDING-014` |
| 4 | Disconnect stops external transfer and preserves local stock | `TopologyChangeBeforePickupReleasesReservation`; `InTransitPauseSaveAndResume` |
| 5 | Reconnection resumes without duplication | `PhysicalMarketConnectivityJourney`; `TopologyChangeBeforePickupReleasesReservation`; conservation assertions in logistics tests |
| 6 | Separate networks cannot use shared-city teleportation | `PhysicalMarketAccessAndDeterministicSelection` separate-market and isolated-warehouse cases |
| 7 | Warehouse, residence, and harbor access is physical | `CapacityPickupDelayAndCompletedDelivery`; `ConstructedResidenceAndMarketAccess`; `PhysicalHarborHandoff` |
| 8 | Road break during delivery cannot complete unreachable transfer | `InTransitPauseSaveAndResume` breaks the final road tick and preserves cargo |
| 9 | Multiple markets, alternate paths, and demolition are deterministic | `PhysicalMarketAccessAndDeterministicSelection`; `ActiveDeliveryUsesDeterministicAlternateRoute`; `TopologyChangeBeforePickupReleasesReservation` |
| 10 | Save/load preserves blocked jobs and economic state | `InTransitPauseSaveAndResume`; `Hansa.Integration.Save` (11 tests); catalog migration fixture |
| 11 | Opening remains recoverable without cheats or fixed order | `PlayableShortageProjection`; `PhysicalMarketConnectivityJourney`; normal placement/removal capture flow |

## Performance

`Hansa.Integration.PhysicalMarketConnectivity.RepresentativeMvpCityPerformance` profiles the authored Lübeck opening with seven road-dependent buildings, 200 full projections in connected and disconnected states, normal road removal/reconstruction, and 120 connected simulation ticks.

Measured on the acceptance host:

- connected projection average: `0.902763 ms`;
- disconnected projection average: `0.886915 ms`;
- simulation average: `0.532376 ms/tick`;
- acceptance budget: `<25 ms` for each measure.

Machine-readable evidence: `Saved/Profiling/physical-market-connectivity.json`.

## Validation actually run

- `HansaEditor Win64 Development` build.
- `Hansa Win64 Development` build.
- `Hansa Win64 Shipping` build and `VerifyShippingExclusion.ps1`; the Shipping receipt and executable contain none of the forbidden editor, automation, test, worker, provider, credential, or staging tokens.
- Logistics 10/10, population 9/9, trade 3/3, save 11/11, content definitions 9/9, authoring architecture 5/5, authoring integration 4/4, mocked generation-worker 10/10, automation protocol 19/19, production-family acceptance 1/1, performance 1/1, playable opening 1/1, and runtime-host integration 3/3.
- Hansa MCP contract suite and two-player authority proof.
- Real rendered production-inspector disconnect/reconnect captures at native 1280×720 and 1920×1080 with sibling semantic TSV evidence.

## Principal changed files

- `Source/Hansa/Public/Definitions/HansaEconomicDefinitions.h`
- `Source/Hansa/Private/Definitions/HansaEconomicDefinitions.cpp`
- `Source/HansaEditor/Private/Definitions/HansaEconomicDefinitionSeedCommandlet.cpp`
- `Source/Hansa/Public/World/HansaLubeckScenarioInitializer.h`
- `Source/Hansa/Private/World/HansaLubeckScenarioInitializer.cpp`
- `Source/HansaEditor/Private/Tests/HansaEconomicDefinitionTests.cpp`
- `Source/HansaTests/Private/World/HansaRuntimeSimulationHostTests.cpp`
- `Source/HansaTests/Private/World/HansaResidenceConsumptionMigrationTests.cpp`
- `Source/HansaTests/HansaTests.Build.cs`
- `Tests/Golden/economic_catalog_v10.json`
- `Tests/Golden/Editor/foundation_sample.schema.json`

## Verification boundary

The real viewport evidence runs through Unreal with rendering and normal player commands, while the target builds prove the final source compiles for Development and Shipping. `VerifyShippingExclusion.ps1` explicitly audits the Shipping target receipt and executable. This pass did not cook/package content or execute a playthrough from an installed packaged build, so packaged gameplay behavior and the cooked/depot content audit remain unverified release gates.

The repository-wide convention audit remains red for existing issues outside this feature: promoted windmill assets and two production source files still contain generation-staging provenance strings, and multiple existing world tests use legacy three-segment names. Prompt 5 introduced no remaining convention violation after its performance test was moved into the accepted `Hansa.Integration.PhysicalMarketConnectivity.*` hierarchy. The Shipping binary exclusion audit is green despite those source/content audit findings.
