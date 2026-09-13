# Enhanced playable MVP baseline — EMVP-P00

## Purpose

This dated reconciliation records the implementation baseline inspected for EMVP-P00 on 2026-09-07. [MVP.md](../MVP.md) is the authoritative scope and acceptance contract; [MVP-enhanced.md](../MVP-enhanced.md) is the remaining delivery plan. This report does not declare an enhanced requirement complete.

The existing S00–S14 work is a substantial deterministic simulation, editor, automation, UI-foundation, and provider-pipeline baseline. It was built against an earlier four-market-city/four-chain scenario and its final release attempt remains blocked. The enhanced milestone converts that foundation into a production-presented, player-operated two-city slice.

## Current implementation compared with the enhanced target

| Enhanced contract | Inspected implementation/evidence | EMVP-P00 disposition |
| --- | --- | --- |
| Buildable Lübeck | `L_Lubeck_MVP` is the only checked-in city map. Deterministic placement, construction, road occupancy, projection Actors, camera, save reconstruction, and fixtures exist. The world foundation and many projections still use `/Engine/BasicShapes` and placeholder topology. | Preserve the authority/projection architecture. Production world art, complete presentations, and real player-camera proof remain required by EMVP-P07–P20 and P30. |
| Rendered, prebuilt, inspectable, tradeable Rostock | `DA_City_Rostock` and Lübeck–Rostock market/route state exist. The trade UI renders a Rostock marker, but no Rostock map or rendered city presentation is checked in. | A market record or map marker is not acceptance evidence. EMVP-P31 must deliver the rendered non-buildable city and its inspection/trade flow. |
| Bread, fish, and planks are the only local buildable chains | The registry contains ten goods, eight recipes, and 14 building definitions, including smithy/tools and brewery/beer. The runtime simulates four earlier chain families. | Retain schema/runtime compatibility, but the enhanced player construction catalog exposes only Grain Farm→Mill→Bakery, Fishery, and Lumber Camp→Sawmill. Other goods need explicit bounded non-local sources under EMVP-P03/P33. |
| Full ten-good market and population loop | Deterministic inventories, production, needs, satisfaction, workforce, growth/decline, upgrade commands, causal prices, market history, and native UI models/tests exist. | This is reusable foundation evidence. Enhanced acceptance still requires ordinary-player flows, complete active-source rules, real assembled UI, balance, and visible progression proof under EMVP-P24/P25/P33/P36. |
| Card drag into the world with authored 3D ghost | `UHansaBuildMenuPresentationModel` constructs a hard-coded card list. `SHansaBuildMenu` exposes fixed target buttons such as `Road target 18,16` and `Fit beside road`. Placement sessions and structured validation exist, but no card-to-viewport drag journey or authored ghost presentation is implemented. | EMVP-P03/P04 replace the presentation contract and player interaction while retaining the command gateway and placement validator. Fixed target helpers remain development-only and cannot appear in the player path. |
| Player-drawn roads | Deterministic Manhattan road-drag expansion and atomic placement batches exist in simulation tests. The current player-facing build menu targets fixed cells and final roads fall back to Engine cube geometry. | Headless path tests are necessary but insufficient. EMVP-P05 must connect real viewport input, live path/cost/validity presentation, accessible alternatives, and production road assets. |
| Production building/road/prop presentations | Bakery and laborer-residence assignments have focused evidence; windmill content exists but has unresolved provenance and catalog-review blockers. The projection Actor deliberately falls back to Engine cubes/cones/spheres. Several road/field assets exist only as source or staging work. | No golden-path fallback is accepted. EMVP-P02 creates the completeness manifest; EMVP-P07–P20 audit, revise, import, and prove every visible role. |
| Visible cargo vehicles | Authoritative Cog/wagon routes, cargo ledgers, transfer events, UI projections, and delivery fixtures exist. No general Cog/cart world-presentation Actor is present. | EMVP-P19/P32 must add verified vehicle assets and read-only world projections correlated to the same stable entity IDs, ticks, cargo, and market events. |
| Production UI and frontend | Native Slate HUD, build menu, inspector, city overview, market, trade, research, scenario, and save/load widgets exist with semantic/controller tests. S14 UI evidence used isolated test surfaces and explicitly lacked a runtime-backed build catalog. No production frontend/start/settings shell is present. | Preserve view models, semantics, and input parity. EMVP-P21–P29 establish the production component system and real assembled screen evidence. Synthetic or isolated captures remain regression aids only. |
| Non-prescriptive 30–60 minute session | The existing `s14-p01-mvp-golden` flow is a fixed scripted sequence that confirms a bread chain and activates preauthored sea/land routes. Deterministic scenario, AI, research, objectives, victory, and recovery mechanics are reusable. | EMVP-P28/P33/P36/P37 must provide optional onboarding, several viable openings, recovery evidence, timed playthroughs, and a prosperity journey rather than treating one automation script as game design. |
| Clean playable Shipping proof | Target/module exclusion scans pass in limited scope. The last release report is blocked by the registry hash mismatch, deterministic golden drift, windmill provenance strings, and a failed Shipping cook on Landmass editor resources. | These remain blockers. EMVP-P01 addresses the current catalog blocker; EMVP-P38/P39 require successful clean build/cook/package, expanded audit, launch, real viewport, and completion evidence. |

## Scope decisions

- Lübeck and Rostock are the only required player-facing cities. Existing Hamburg and Lüneburg records may remain as compatible, non-gating data/tests; additional required cities are post-MVP.
- Rostock is rendered and inspectable but not buildable. A second buildable city is post-MVP.
- Bread, fish, and planks are the only locally buildable production chains. Existing smithy and brewery data may remain supported but is not exposed as enhanced golden-session construction content.
- The Lübeck–Rostock sea route is the required intercity player journey. Existing land-route simulation remains compatible foundation coverage, not a required player-facing MVP flow.
- Earlier architecture, editor/game/automation parity, deterministic authority, staging/promotion, save, multiplayer proof, and Shipping-exclusion requirements remain in force.

## Evidence interpretation

The matrix in `MVP.md` controls acceptance. Dated S14 reports and current unit/contract tests establish reusable foundation evidence and known blockers. They do not automatically pass broader enhanced rows. Visual and interactive rows require the actual assembled game, ordinary input path, correlated structured state, and original-resolution evidence. The implementation may not substitute:

- a market-only city record for a rendered city;
- a direct command or fixed semantic helper for a player interaction;
- a source-art render or generated mockup for a shipping asset;
- an isolated Slate test surface or synthetic buffer for the real game viewport;
- a route row for a visible cargo vehicle;
- a successful target scan for a cooked, packaged, launched build;
- one scripted automation sequence for a non-prescriptive 30–60 minute player session.

EMVP-P02 replaces the broad presentation observations above with the checked-in, machine-validated
[visible-content manifest](../../Tests/Golden/enhanced_mvp_visible_content_v1.json). Its audit method,
status vocabulary, ownership rules, and validation commands are documented in
[VisibleContentCompleteness.md](VisibleContentCompleteness.md). The manifest is the authoritative
inventory for remaining golden-session presentation work; this baseline remains the dated narrative
snapshot.

EMVP-P03 replaces the hard-coded build-card presentation noted above with the authored, compiled
[construction catalog](ConstructionCatalog.md). Bread, fish, and planks now expose their separately
ordered building chains through native Slate and semantic IDs; costs, footprint, workforce, flows,
locks, and availability are derived from authoritative definitions and runtime queries. Catalog v3
records the schema migration while retaining a reproducible catalog-v2 lineage proof.

## Files intentionally unchanged by EMVP-P00

EMVP-P00 changes scope and acceptance documentation only. Runtime definitions, fixtures, stable IDs, immutable fixture versions, assets, registry pins, and test goldens are unchanged. Later prompts must version or migrate them through the existing parity contracts rather than reinterpreting historical evidence.
