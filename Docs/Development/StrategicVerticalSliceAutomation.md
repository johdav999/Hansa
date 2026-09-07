# S10-P04 Strategic Vertical Slice Automation

S10-P04 extends the Development-only automation surface across the playable runtime rather than adding a second simulation. The flow starts in the authored Lübeck grain-shortage scenario and uses the normal placement, route, research, merchant-AI, scenario-evaluation, projection, and state-hash paths.

## Fixture contract

Two exact fixture IDs are allowlisted:

- `strategic_vertical_slice_seed_alpha_v1` — campaign seed `4C554245434B4752`
- `strategic_vertical_slice_seed_beta_v1` — campaign seed `4C554245434B4753`

The seed is selected only at new-scenario initialization. It cannot be changed after load, and arbitrary wire-provided seeds are rejected by construction. Replaying either fixture must produce the same final projection fingerprint, AI decision trace, causal event list, research state, and objective state for that seed. Different seeds are allowed to choose different deterministic AI tie breaks.

## Stable automation flow

`Tools/HansaMcp/scripts/strategic-vertical-slice-flow.js` performs the following sequence for both seeds and then replays each seed:

1. Load the exact fixture ID.
2. Activate `Strategic.Action.Build` to place an authored road through `FHansaPlaceBuildingCommand` and wait for `strategic.building_completed`.
3. Wait for the typed Lübeck grain stock/reserve deficit and activate `Strategic.Action.Diagnose`.
4. Activate `Strategic.Action.QueueResearch`, which submits `FHansaQueueResearchCommand` for `Technology.Commerce.MarketReports`.
5. Activate `Strategic.Action.StartRoutes`, which submits `FHansaSetRouteActiveCommand` for player routes 1 and 2.
6. Wait for the applied research effect, at least two merchant decisions, two active player routes with completed trade legs, and `Victory.TradeNetwork`.
7. Assert every checkpoint, issue typed market/research/AI/scenario/evidence queries, and capture the final native Slate proof surface.
8. Reload the same seed and compare state hash, AI decisions, and objective state with the first execution.

No coordinate clicking, sleeps, direct stock mutation, direct objective mutation, or internal state polling is used. The route-recovery checkpoint follows the authored victory contract (both canonical player routes active and completed trade legs), so it does not overfit to one cargo-transfer event when market reserves legitimately alter a load/unload result.

## Evidence

The `strategic.evidence` typed query and screenshot query snapshot contain:

- fixture ID, exact campaign seed, tick, projection state hash, and event/AI counts;
- bounded merchant-AI decisions with decision tick, ordinal, goal, chosen option, outcome, and reason;
- player research queue/completion state and applied stable effect contracts;
- authored scenario outcome, winning victory ID, every victory path, objective values, targets, sustain progress, and met state;
- causal building, construction, route, cargo, research-queued, and research-completed events.

Native 1280×720 and 1920×1080 contract captures are written under `Saved/TestEvidence/Automation/S10P04/`. Per-seed structured evidence is written under `Saved/TestEvidence/StrategicVerticalSlice/<fixture-id>/strategic-victory.json`.

The proof surface is reconstructed from native Slate widgets using the established Hansa UI tokens. It is Development-only and contains no generated raster assets or baked dynamic text.

## Verification

Build:

```powershell
.\Scripts\Build.ps1 -Target HansaEditor -Platform Win64 -Configuration Development
```

Focused golden test:

```powershell
.\Scripts\RunAutomationTests.ps1 -TestFilter 'Hansa.Architecture.Automation.StrategicVerticalSlice' -SkipBuild
```

Interactive MCP run requires an explicitly enabled Development process with `HANSA_AUTOMATION_PIPE` and `HANSA_AUTOMATION_TOKEN`, then:

```powershell
node Tools/HansaMcp/scripts/strategic-vertical-slice-flow.js
```

This is a strategic vertical-slice golden flow, not the final full-MVP golden test. It deliberately stops at one deterministic trade-network victory while retaining the other authored victory paths as separately tested alternatives.
