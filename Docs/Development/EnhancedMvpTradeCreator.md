# EMVP-P26 — Baltic route creation

## Component inventory and states

The existing P21 native shell, navigation, action buttons, focus rings, typography and status surfaces remain the shared components. New references cover the composed route workspace, ordered stop editor, and departure review. All are non-shipping references; dynamic content is native Slate.

| Component | States and behavior |
| --- | --- |
| Screen shell / route navigation | Browse, new draft, retained draft, selected route; close and keyboard/controller navigation |
| Baltic chart | Lübeck–Rostock focus, selected route, traveling vessel; other scoped cities remain in the route directory |
| Ordered stop editor | Selected stop, add/remove/reorder, city/good/action choice, quantity and source reserve |
| Cog chooser | Available, safely reassignable stopped route, unavailable cargo/in-transit/rival |
| Departure review | Name, capacity, time, upkeep, expected cash result, report uncertainty, validation failure and retry |
| Controls | Default, hover, pressed, selected, disabled with explanation, keyboard/controller focus |
| Feedback | Draft, warning, validation error, created/active, partial/missed/delivered cargo |
| Icons / decoration | Shared native geometry and restrained chart ruling; no new shipping raster assets |

Generation: built-in ImageGen, composed reference first, then one call per distinct new reusable component. Target 1536×1024 native pixels; inspect originals and preserve prompt records. Palette and typography follow `Docs/UIDesignBrief.md`.

## Economic contract

Cargo actions transfer inventory; they do not execute market purchases or sales. The review must not claim price arbitrage as realized profit. Expected cash result therefore includes travel upkeep and explicitly states that cargo has no automatic sale revenue. Remote report freshness is advisory and must come from the player's known-information query.

## Implementation and validation

Implemented on 2026-09-09. The ordinary game UI creates a new named route, chooses/reassigns an owned Cog, edits up to eight ordered stops, validates the complete plan, and activates through typed commands. The native route directory reports lifecycle, cargo, last transfer and ownership. City reports use player-known information; the chart draws its coastline, route, port labels and moving Cog natively.

Drafts survive close/reopen. Invalid names, duplicate/unreachable stops, missing delivery actions, unavailable/carrying Cogs and changed runtime state explain the problem and preserve the draft. Review is revalidated on departure. Reassignment cancels the prior stopped route and creates the new route in one atomic gateway batch; repeated activation cannot duplicate it. Name entry accepts native keyboard text, while stop edits, review and departure support controller activation and non-drag navigation.

## Open and test

Launch a fresh Development game with `Scripts/LaunchGuiPreview.ps1`; an already-running game retains its old DLL. Open **Trade map → New route**, or **City overview → Market → Open full market → select a good → Begin route**. Configure stops and reserves, enter a name, select the Cog, then **Review voyage → Create and activate**. Resume time to observe cargo movement. If the Cog is busy, its current route must return, unload and pause at its first stop before reassignment.

```powershell
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.UI.Trade -SkipBuild
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Integration.Save -SkipBuild
.\Scripts\CaptureGuiRepair.ps1 -TestFilter Hansa.UI.TradeCreator.RealViewport -Width 1280 -Height 720 -UiScale 1.0
python Scripts/ValidateTradeCreator.py
```

## References and native reconstruction

Four selected built-in ImageGen outputs are saved at **1536×1024**, with sibling final prompt and inspection records:

| Component reference | Project path |
| --- | --- |
| Composed workspace / style anchor | [workspace](../Images/UI/TradeP26/trade-p26--workspace--reference--1536x1024--v1.png) |
| Ordered stop editor | [ordered stops](../Images/UI/TradeP26/trade-p26--ordered-stops--reference--1536x1024--v1.png) |
| Departure review | [review](../Images/UI/TradeP26/trade-p26--departure-review--reference--1536x1024--v1.png) |
| Regional chart | [chart](../Images/UI/TradeP26/trade-p26--chart--reference--1536x1024--v1.png) |

These are visual references, not imported production textures. All shipping UI is native Slate with shared P21 palette, typography, actions and focus treatment. No raster was resized, stretched, cropped into controls, or imported into Content. The full prompt set and SHA-256 records are beside the references in [TradeP26](../Images/UI/TradeP26/verification.json).

The original-resolution comparison retained the references' maritime navy/linen contrast, restrained serif hierarchy, ordered cargo plan and explicit departure review. The implementation uses the shared native primary-action style rather than copying generated button colors. The composed generator invented extra example routes and inaccurate broad geography; those details were rejected. The chart reference is illustrative; the native chart explicitly identifies itself as generalized. Hamburg and Lüneburg remain in existing scoped data/routes; the new creator focuses on Lübeck and Rostock.

Native inspection caught and fixed striped procedural land fill, low-contrast focused name text, large-text heading clipping, report labels crossing dark water, weak departure emphasis and scroll-hidden validation. The final evidence includes default and high-contrast/large-text/reduced-motion states. At small resolutions with large text the editor and review scroll; validation and departure controls remain visible. Ultrawide layouts retain more negative space than the reference. These checks establish feature readiness, not a blanket AAA art-direction or full-game release approval.

## Verification

- Development build passed: `20260909-130952468-build-HansaEditor-Win64-Development`.
- 21 deterministic/regression tests passed: Trade 5, Market 5, SelectedGood 2, Save 9. Coverage includes actual new-route delivery, command rollback, repeated activation, retained drafts, market-good entry, invalid/busy recovery, saved Unicode route names, prior save formats 1/2 and idempotent migration.
- 12 real-viewport runs passed across **1280×720, 1920×1080, 2560×1440, 3440×1440**, each at **80%, 100%, 140%** scale. All twelve create route 4 through native keyboard/controller controls and deliver cargo to Rostock through the ordinary game clock; no debug route creation or delivery command is used.
- **108 native PNGs** plus semantic TSVs are archived under [Native](../Images/UI/TradeP26/Native/manifest.json). The validator checks original dimensions/hashes, positive deliveries, and visible departure buttons within viewport bounds at all twelve profiles.
- Shipping build and executable/receipt exclusion audit passed: `20260909-131039079-shipping-exclusion-Win64`. Full cooked/depot package certification remains a separate release gate.

Build/test/run provenance and reference hashes: [verification.json](../Images/UI/TradeP26/verification.json). Native-scale visual comparison browser: [comparison.html](../Images/UI/TradeP26/comparison.html).

## Persistence and editor parity

No authorable good, vehicle, route definition, gameplay invariant, provider integration or registry schema changes. Creation reuses existing `FHansaCreateRouteCommand` / `FHansaCancelRouteCommand` validation. Player route names are bounded cosmetic campaign metadata, excluded from AI definition authoring and never used as gameplay identity. Save format 3 appends that metadata; explicit v1/v2 migrations preserve authoritative state. See [SaveEnvelope.md](SaveEnvelope.md) and [TradeRoutes.md](TradeRoutes.md). The native semantic contract and deterministic editor/game fixtures are updated in the same implementation.

