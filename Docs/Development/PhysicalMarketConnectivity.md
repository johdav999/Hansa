# Physical market connectivity — prompt 4

Prompt 4 completes the player, editor, and automation integration for the local road economy. `UHansaBuildingDefinition` schema v4 authors `bProvidesMarketAccess`; catalog v9 (`F1D0D88180A4C342`) marks the reviewed Market building as the provider and retains catalog-v8 reconstruction evidence. Lübeck's city inventory is bound to building 14, Rostock retains its valid market-only stock endpoint, and the playable bootstrap can afford a road reconnection.

Every completed road-dependent building receives a proactive `FHansaBuildingWorldProjection` access result, even without an active delivery request. The result carries connection state, cause/message/remedy keys, selected market, road distance, and blocked-delivery evidence. The production inspector renders this state and exposes the same values through `Inspector.Logistics.MarketAccess`. It distinguishes no adjacent road, no operational market, inaccessible market endpoint, severed network, paused delivery, missing recipe input, and full output storage.

Player and automation recovery use the normal `building.remove` and `building.place` command paths. The acceptance journey removes the two road-spine cells that isolate the bakery, observes `SourceNotConnectedToMarket` and a paused delivery, rebuilds one segment, completes construction, and confirms that the same request resumes to Market 14. Typed automation queries are `building.list`, `building.market_access`, `logistics.requests`, `logistics.jobs`, and `logistics.path`.

Save loading registers explicit catalog migrations from reviewed catalogs v7 through v9. A prior save is verified with its recorded definition hash before the migration is applied; the migration name is persisted in save history and the authoritative hash is then recalculated against catalog v10. Unknown content hashes remain incompatible.

Real offscreen gameplay captures were produced at native 1280×720 and 1920×1080 for both disconnected and reconnected states. Each PNG has a sibling TSV semantic snapshot under `Saved/ProductionInspector/`; no generated raster asset or screen redesign was required.

Prompt 5 acceptance advanced the reviewed catalog to v10, made Fishery require both shoreline and road access, and added a schema invariant for every production building. The complete acceptance matrix, performance results, builds, Shipping audit, and verification boundary are recorded in [PhysicalMarketConnectivityAcceptance.md](PhysicalMarketConnectivityAcceptance.md).

Focused verification:

```powershell
pwsh -File Scripts/Build.ps1 -Target HansaEditor -Platform Win64 -Configuration Development
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Integration.RuntimeSimulationHost.PhysicalMarketConnectivityJourney -SkipBuild -NoZenDdc
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Architecture.Automation.MvpGoldenEndToEnd -SkipBuild -NoZenDdc
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Integration.Authoring.EconomicAssetReload -SkipBuild -NoZenDdc
pwsh -File Scripts/RunHansaMcpTests.ps1
pwsh -File Scripts/CaptureProductionInspector.ps1 -Width 1280 -Height 720
pwsh -File Scripts/CaptureProductionInspector.ps1 -Width 1920 -Height 1080
```
