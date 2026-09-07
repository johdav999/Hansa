# MVP golden MCP gate

S14-P01 is the final development-only MCP golden path for the integrated MVP. Its stable test ID is `s14-p01-mvp-golden`; its exact fixture is `lubeck_grain_shortage_v1` version 4 with the fixed Lübeck campaign seed. The game remains authoritative. The sidecar sequences public operations but cannot set stock, prices, progression, AI state, objectives, or save hashes.

The fixture ID also names the immutable S04 actor-free market-recovery contract. To preserve that supported API, an ordinary session without `evidence` continues to list/load its original headless profile. Explicitly negotiating `evidence` selects the version-4 playable-runtime golden profile under the same canonical scenario identity. `fixture_list` advertises exactly one descriptor matching the negotiated profile; it never returns duplicate IDs. Reset retains the active profile because the capability set is fixed for the session.

## Entry points

With an explicitly enabled development game and fresh pipe/token environment, either call the MCP tool:

```json
{"testId":"s14-p01-mvp-golden","bundleId":"s14-p01-mvp-golden","captureScreenshots":true}
```

or run the direct named-pipe smoke:

```powershell
npm --prefix Tools/HansaMcp run smoke:golden
```

The reusable orchestration is `Tools/HansaMcp/src/golden-flow.js`; both entry points call it. `test_run` owns its game session and always attempts `session_stop`.

The CI-owned live runner builds when needed, launches a hidden offscreen-rendered Development game with a fresh token and pipe, waits observably for that exact pipe, runs the same driver, verifies `bundle.json` is complete, and stops only the process it created:

```powershell
pwsh -NoProfile -File Scripts\RunMvpGoldenMcpTest.ps1
```

## Golden path

1. Discover automation protocol 1.0 and require session, capability discovery, gameplay query/command, fixture control, semantic UI, screenshots, observable waits, and evidence.
2. Start a `FixtureControl` session, list the canonical fixture, load it, query its initial state, reset it, and prove the reset state hash is identical.
3. Find `Market.Good.Grain` structurally, assert its warning state, activate `Market.Action.DiagnoseGrain`, wait for `Market.Status.GrainDiagnosed`, and query typed scarcity/demand causes.
4. Activate `BuildMenu.Action.ConfirmBreadChain`, wait for authoritative construction completion, then activate the two relief routes and `Technology.Commerce.MarketReports` through their semantic actions.
5. Wait for merchant-AI decisions and in-transit player cargo. Create and load only the fixed `manual` slot, then assert authoritative hash equivalence, projection equivalence, deterministic continuation, and required gameplay-slice coverage.
6. Wait for the research effect, route recovery, and authored victory. Query causal events, AI decisions, research, objectives, seed, content/fixture hashes, and final state hashes.
7. Capture native 1280×720 and 1920×1080 evidence without post-capture resizing, read bounded correlated logs, and persist the synchronized evidence bundle.

The flow contains no fixed sleep. Semantic waits observe registry revisions; gameplay waits advance a bounded deterministic tick at a time through the normal simulation surface.

## Semantic checkpoints

The release bundle requires these stable selected states:

- `BuildMenu.Status.BreadChain`
- `Market.Status.GrainDiagnosed`
- `TradeRoute.Editor.Status.Delivered`
- `Research.Status.MarketReports`
- `HUD.Status.MerchantAI`
- `SaveLoad.Status.RoundTrip`
- `Scenario.Status.Victory`

The aliases are registered on the existing strategic proof screen and invoke the same authoritative commands as its S10 semantics. Dynamic labels, values, prices, and status are native text/state, not raster content.

## Evidence contract

`evidence_bundle_create` accepts only a bounded bundle ID, the exact S14 test ID, the negotiated MCP protocol version supplied by the server, and up to 128 `{id, passed}` assertions. The endpoint writes:

```text
Saved/TestEvidence/Automation/S14P01/<bundleId>/
  bundle.json
  query-snapshot.json
  semantic-ui.json
  logs.json
  assertions.json
```

`bundle.json` records the evidence schema, MCP/wire/automation protocol versions, fixture ID/version, registry-derived content hash, fixture hash, seed, initial/final/projection/save hashes, and screenshot paths/SHA-1 values. The sibling files retain the synchronized projections needed to diagnose a failure. `complete` is true only when caller assertions, all required semantic checkpoints, verified save round trip, and both native sizes are present.

If orchestration fails, MCP returns `GoldenTestFailed` with its phase and checkpoint and attempts an incomplete `-failure` bundle before closing the session. The direct smoke prints the same bounded failure object to stderr and exits nonzero.

## Automated verification

```powershell
pwsh -NoProfile -File Scripts\RunHansaMcpTests.ps1
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Architecture.Automation.MvpGoldenEndToEnd
pwsh -NoProfile -File Scripts\RunMvpGoldenMcpTest.ps1 -SkipBuild
```

The Node suite proves tool schemas, negotiated-version recording, success orchestration, failure checkpoint preservation, framing, and the full fake-endpoint flow. The Unreal test drives the real `UHansaRuntimeSimulationHost`, semantic registry, fixed-slot save envelope, research/AI/scenario systems, query/log projections, and native screenshot persistence. The live runner proves the MCP transport and true Slate captures against the same runtime. `Scripts/InvokeCI.ps1` runs both focused and live gates explicitly even when its general test filter is overridden.

The strict reviewed economic-registry hash remains a prerequisite. A content/hash mismatch blocks fixture initialization and therefore blocks S14 completion; the golden test does not bypass or rewrite that release-safety check.
