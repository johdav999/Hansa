# EMVP-P34 — two-city trade journey

Status: implemented and verified against the production-v8 economy; Rostock visual content retains its existing staging gate.

Product profile: Hansa native Unreal game, local Development build, player house in the deterministic Lübeck production-v8 scenario. Launch with `Scripts/CaptureGuiRepair.ps1 -P31Candidate -ReadOnlyZenDdc -TestFilter Hansa.UI.TradeJourney.RealViewport`. Rostock uses its existing staged visual quarter; this does not promote P31 or P33 content. Evidence goes to `Saved/P34` and archived references under `Docs/Images/UI/TradeJourneyP34`.

Flows: F1 compare Lübeck shortage and known Rostock supply; F2 review an import circuit and protected reserve; F3 observe real loading, voyage and delivery; F4 inspect inventory, need and price feedback; F5 recover invalid/empty/oversized drafts, pause/edit/cancel and restore a save.

| Issue | Severity | Flow | Observed / expected | Acceptance | Status |
|---|---|---|---|---|---|
| P34-001 | P1 | F1–F2 | Begin route always exports from Lübeck, even for an import opportunity. | A Lübeck shortage seeds a Rostock load / Lübeck unload draft. | Verified |
| P34-002 | P1 | F2–F3 | Empty-cargo validation inspects only the first circuit and rejects return-leg imports. | Cyclic matching loads/unloads validate, missing unload remains rejected. | Verified |
| P34-003 | P2 | F5 | Existing runtime cancellation has no normal UI action. | Owned stopped empty vessels can cancel; loaded/traveling/rival routes cannot strand cargo through this control. | Verified |
| P34-004 | P2 | F5 | Edit gateway errors render route-plan error None. Dirty draft also prevents pausing. | Actionable edit failure preserves draft; pause remains available to prepare an edit. | Verified |
| P34-006 | P2 | F1/F4 | Repeated city/tab selection returns Unhandled from a clicked button, triggering a native Slate ensure. | Repeated native city and tab activation is a handled no-op. | Verified |
| P34-005 | P2 | F2–F5 | Reserve status reads an unset route flag. | Draft reserve warnings use known stock, with uncertainty explicit. | Verified |

No new visual system or raster assets: reuse approved trade-map shell, chart, route rows, cargo controls, review, feedback and focus states. Cancellation uses the existing secondary action component. Functional fixes preserve approved styling and do not require a redundant ImageGen pass.

## Player journey

Start a new Lübeck game, open the city overview, choose Market and the full market table, and select Bread. Compare the known Rostock market report through the city selector, then return to Lübeck and choose Begin route. A shortage now prepares an import: unload in Lübeck, load in Rostock, with a 5-unit source reserve. Review the two-circuit cargo preview and activate. The Cog begins at its actual port; its first empty unload is an explicit missed action, not invented cargo.

Select a route stop and Visit selected stop to watch its berth. Use Return to Lübeck for the return leg. Real loading and unloading receipts identify the same vehicle, inventory, good, quantity and tick as the simulation events. Both cities' market prices react to changed inventories/incoming supply. Lübeck households consume imported Bread; Rostock remains the existing report-based trade city, with background consumption and no invented residential population report.

Pause is available at port. Edits require an empty hold and the current port first in the itinerary; use Move up when pausing at the second port. Draft edits no longer prevent pausing. Cancellation requires an owned, stopped, empty vessel, preserving the player's ability to reuse it without stranding cargo. A cancelled route cannot restart; create a new route. The native UI returns focus to New route and keeps the cancellation state readable while returning focus to New route.

## Deterministic evidence

Fixture: `Tests/Fixtures/trade_journey_p34_v1.json`. The normal controls request 20 units of Bread in each direction of the import action pair, with a 5-unit Rostock reserve. The real first load is partial: **19 units at tick 12**, leaving **5 units** in Rostock. The Cog unloads **19 units in Lübeck at tick 23**, leaving its hold empty. These are actual inventory movements, not automatic sale revenue.

The 80-tick fixture runs twice with identical fingerprints and compares a separate no-trade control. Imported Bread increases cumulative fulfilled consumption from **37,000 to 54,800 milli-units**. Both cities' prices respond, the source inventory decreases, and save/load preserves the exact authoritative fingerprint. Recovery tests cover all-load rejection, oversized quantity rejection and correction, insufficient stock/zero load, protected reserve, traveling cancellation rejection, failed edit text, dirty-draft pause, safe cancellation, rival ownership, and aged report uncertainty.

Native UAT enters through New game and uses focus + keyboard activation of real controls, bounded waits on typed simulation states and streaming completion, and real game-viewport captures. Each PNG has a synchronized text packet with semantic bounds, inventories, consumption, prices, fingerprint, cargo observation and route event history. The validator joins transfer tick/quantity/good/city to domain events and checks that every display mode yields identical simulation fingerprints.

## Visual inspection and assets

Component inventory: existing trade shell and route navigation; geographic chart; ordered stop list; cargo quantity/reserve controls; departure review; route state and transfer receipt; pause/save/cancel controls; report/validation feedback; native focus indicators. No new decorative imagery or production raster is introduced. Default, selected, focused and disabled behavior uses the approved native components; invalid drafts and older reports retain explicit text.

Reference inspected at original resolution: `Docs/Images/UI/TradeP26/trade-p26--departure-review--reference--1536x1024--v1.png` (existing ImageGen reference; sibling prompt record retained). Native review preserves its information hierarchy, capacity/upkeep explanation, explicit reassignment, inventory-transfer limitation, and separate edit/activate actions. The existing implementation uses the shared parchment/ink style; it is not a pixel-identical reproduction of the reference artwork. Original-resolution inspection also checks the cancellation flow and maximum UI scale. Generated reference artwork remains non-shipping; final P34 images are native viewport evidence, not production textures.

Target evidence: 1280×720, 1920×1080, 2560×1440, 3440×1440 at 100%, plus 1280×720 at 80% and 140%. PNGs are saved at native dimensions without resampling. Paths: `Docs/Images/UI/TradeJourneyP34/`; machine-readable evidence: `Docs/Development/TradeJourneyP34/verification.json`. No new ImageGen prompt set or imported asset applies to these functional changes.

## Boundaries

Production economy catalog v8 is unchanged. P33's balance candidate remains staged pending its earlier approval gate. The visible Rostock quarter still requires `-P31Candidate`; its production promotion is outside P34. Existing P30/P31 world-art limitations remain. This verifies the trade journey and UI behavior; it does not certify final AAA world art or a new packaged Shipping release.

Commands:

```powershell
./Scripts/RunAutomationTests.ps1 -SkipBuild -NoZenDdc -TestFilter Hansa.UI -EngineRoot H:/Unreal/UE_5.8
./Scripts/CaptureGuiRepair.ps1 -P31Candidate -ReadOnlyZenDdc -TestFilter Hansa.UI.TradeJourney.RealViewport -Width 1920 -Height 1080 -EngineRoot H:/Unreal/UE_5.8
python Scripts/ValidateTradeJourney.py
./Scripts/VerifyShippingExclusion.ps1 -EngineRoot H:/Unreal/UE_5.8
```

## Verification results (2026-09-09)

- Development editor build succeeded; no runtime/editor dependency changes or new gameplay schemas.
- `Hansa.UI`: **63/63 passed** (`Saved/BuildArtifacts/20260909-202119070-automation-Hansa.UI`).
- Six display configurations: **6/6 native journey runs passed**, **72 screenshots** with synchronized state/event packets. Four base resolutions and both 80%/140% scale bounds are archived. A separate 4K run also passed; the supported ultrawide replaces it in the final evidence matrix.
- `python Scripts/ValidateTradeJourney.py`: **passed**, verifying native pixels, opaque capture alpha, cargo conservation at loading/unloading, domain-event joins and identical per-stage fingerprints across the display matrix.
- Shipping build/exclusion audit passed (`Saved/BuildArtifacts/20260909-202736382-shipping-exclusion-Win64/result.json`). This is a binary/module exclusion audit, not a new full packaged-content release.
- Original-resolution review: existing ImageGen departure-review reference; native 720p review and unload; native cancellation at 140%. The final cancellation focus correction returns to New route and scrolls the receipt/remedy area to the end. Existing world geometry remains visibly provisional as documented above.
- `git diff --check` passed for the modified tracked UI files.

Native run artifacts: `20260909-202103488-gui-repair-1280-720`, `20260909-202144881-gui-repair-1920-1080`, `20260909-202226318-gui-repair-2560-1440`, `20260909-202309684-gui-repair-3440-1440`, `20260909-202354087-gui-repair-1280-720` (80%), `20260909-202643455-gui-repair-1280-720` (140%, final cancellation focus/scroll check). See the archived per-capture packet for authoritative values; no screenshot-only completion claim is used.
