# Trade presence acceptance matrix

Status date: 2026-09-21. `Verified` requires recorded executable evidence. `Implemented` means code/content exists but the complete prompt gate has not yet been rerun. `Deferred` means intentionally outside the current prompt. `Blocked` names a concrete dependency.

| ID | Prompt | Requirement / exit evidence | Status | Evidence or next owner |
| --- | --- | --- | --- | --- |
| TR-01-A | TR-01 | Identify authoritative types, commands, ledgers, phases, projections, editor coverage, saves, and tests. | Implemented | `TR-01.md` baseline map. |
| TR-01-B | TR-01 | Deterministically characterize whether route load/unload changes money. | Implemented | `Hansa.Simulation.Trade.ForeignRouteSettlementCharacterization`; verification command recorded in `TR-01.md`. |
| TR-01-C | TR-01 | Existing trade, market, save, UI, and Shipping checks pass. | Blocked | Core trade/market/trade-map/focused-save/exclusion checks pass, but the trade-creator delivery assertion and two scenario save fixtures are red; see `TR-01.md`. |
| TR-01-D | TR-01 | Every conflict has a decision or named owner. | Implemented | `Decisions.md`, TP-BLOCK-001 through TP-BLOCK-004. |
| TR-02-A | TR-02 | Authorable stage/policy/capability definitions compile and hash identically under reversed discovery. | Verified | `Hansa.Editor.TradePresence.DefinitionsDeterminismAndValidation`; catalog v33 pin and evidence in `TR-02.md`. |
| TR-02-B | TR-02 | House/city presence state, queries, next stage, unmet requirements, and deterministic save migration. | Verified | `Hansa.Integration.TradePresence.SeedQuerySaveAndInertTick`, `Hansa.Integration.Save.SyntheticPriorSchema`, and `Format2RouteLabelMigration`. |
| TR-02-C | TR-02 | Foundation is inert: no cargo, money, construction, or market behavior changes. | Verified | Inert-tick integration and `Hansa.Simulation.Trade.ForeignRouteSettlementCharacterization`; no capability-consuming commands or systems were added. |
| TR-03-A | TR-03 | Berthed owned ship can execute explicit visiting buy/sell commands through ordinary controls. | Verified | `Hansa.Integration.TradePresence.VisitingSpotTrade`; native market-card and semantic-input evidence in `TR-03.md`. |
| TR-03-B | TR-03 | Conservation, atomicity, partial/rejected/idempotent outcomes, price timing, and rollback pass. | Verified | Focused integration coverage for full, partial, missed, rejected, stale, duplicate, and all four bounded-resource cases; settlement contract in `TR-03.md`. |
| TR-03-C | TR-03 | Save/reload, stale review, semantic input, and explicit uncertainty pass without changing legacy routes. | Verified | Focused integration save/reload and semantic controls; `Hansa.UI.MarketTable`; `Hansa.Simulation.Trade.ForeignRouteSettlementCharacterization`. |
| TR-04-A | TR-04 | Establish/construct/complete/inspect/close one station with stable station, factor, inventory, and lease identities. | Deferred | TR-04. |
| TR-04-B | TR-04 | Costs, site, upkeep, state, save/replay, and negative authorization cases are transactional. | Deferred | TR-04. |
| TR-04-C | TR-04 | Approved world presentation and real viewport evidence; Rostock stays non-buildable outside the station site. | Deferred | TR-04; requires Hansa model workflow if no approved asset exists. |
| TR-05-A | TR-05 | Factor orders acquire/release goods over time using real city stock, station capacity, and player money. | Verified | Focused station-order integration and policy tests; see `TR-05.md`. |
| TR-05-B | TR-05 | Priority/reservations prevent overspend and overcommit; all partial/blocked states are observable. | Verified | Focused station-order integration and policy tests; see `TR-05.md`. |
| TR-05-C | TR-05 | Orders, history, save/reload, UI, schema policy, and deterministic replay pass. | Blocked | State, migration, schema and native semantic controls pass; real viewport and remote-client projection gaps TR05-UAT-01/TR05-NET-01 remain. See `TR-05.md`. |
| TR-06-A | TR-06 | Route actions explicitly distinguish market commerce from ship/station inventory transfer. | Verified | Append-only explicit station/home actions, legacy characterization and save-16 migration; `TR-06.md` and `TR-06/evidence.json`. |
| TR-06-B | TR-06 | Complete market ↔ station ↔ Cog ↔ destination physical chain conserves goods and reservations. | Verified | Focused buffered-flow conservation, reverse factor sales, reserves and capacity checks in `TR-06.md`. |
| TR-06-C | TR-06 | Route/UI/event correlation, pause/cancel/partial/save cases pass without false profit labels. | Blocked | State, save and native semantic controls pass; real viewport TR06-UAT-01 and inherited remote projection TR05-NET-01 remain. See `TR-06.md`. |
| TR-07-A | TR-07 | Contributions derive once from accepted domain events with authored caps/windows/anti-farming. | Implemented | Accepted spot, route-market and station-order events use a per-presence sequence watermark, saturating lifetime counters and bounded causal history; see `TR-07.md`. |
| TR-07-B | TR-07 | Merchant-office upgrade is atomic, explainable, preserves identities/state, and grants only authored capabilities. | Implemented | Typed review/fund commands, candidate-ledger payment, construction completion, native inspector and focused integration coverage; see `TR-07.md`. |
| TR-07-C | TR-07 | At least one alternative commercial pattern can advance; replay/rejection/save cannot duplicate progress. | Implemented | Three lawful transaction sources share one attribution contract; rejection rollback and funded save/reload are covered by `Hansa.Integration.TradePresence.MerchantOfficeProgression`. Real viewport UAT remains unclaimed. |
| TR-08-A | TR-08 | Warehouse, Market, and Harbor branches grant distinct real typed capabilities. | Verified | Authored exclusive branch graph and focused definition/runtime tests; see `TR-08.md`. |
| TR-08-B | TR-08 | Costs, exclusivity, stale review, respec, migration, and transactional replay pass. | Verified | Atomic command, save 18 migration, fingerprint 30, stale/exclusive/respec/save coverage; see `TR-08.md`. |
| TR-08-C | TR-08 | Native comparison UI explains concrete opportunity cost at supported scales/inputs. | Verified | Native Slate comparison, stable semantic controls and controller focus at 1280×720 compact and 1920×1080 wide, separate confirmation, and two inspected ImageGen references; executable coverage is in `Hansa.Integration.TradePresence.MerchantOfficeProgression`. |
| TR-09-A | TR-09 | Authorized construction wholly inside an active leased plot succeeds through the normal build flow. | Verified | `Hansa.Integration.TradePresence.LeasedForeignConstruction`; save round trip and evidence in `TR-09.md`. |
| TR-09-B | TR-09 | Outside/boundary/category/stage/suspended cases reject without mutation. | Verified | Focused integration covers boundary, category, stage and suspended rights with fingerprint rollback; see `TR-09.md`. |
| TR-09-C | TR-09 | Local logistics/workforce/economy stay authoritative and the remainder of the city stays autonomous. | Implemented | Foreign builds spend player money/station stock and preserve city road ownership; native projection is implemented, but real viewport UAT remains unclaimed. |
| TR-10-A | TR-10 | At least one scoped privilege and one funded shared city project have real effects. | Verified | Additional bounded lease and delayed Public Granary bread-reserve effect consume exact house/station resources; focused automation and save/load evidence in `TR-10.md`. |
| TR-10-B | TR-10 | Governance remains scenario/charter gated; disallowed cities reject forged or volume-only transitions. | Verified | Rostock forged charter rejects with fingerprint rollback while the same scenario permits the authored Hamburg charter; see `TR-10.md`. |
| TR-10-C | TR-10 | Transactional transfer contract and UI wording distinguish presence, privilege, partnership, and governance. | Implemented | Native projection/semantics and explicit non-transfer contract are implemented; confirmation-button/runtime-host binding and real viewport UAT remain blocked, recorded in `TR-10.md`. |
| TR-11-A | TR-11 | AI operates trade, station, orders, route, progression, and permissions only through normal commands/resources. | Deferred | TR-11. |
| TR-11-B | TR-11 | Human/AI contention resolves once and deterministic replay matches. | Deferred | TR-11. |
| TR-11-C | TR-11 | Player projections do not leak private AI cargo, orders, or reports. | Deferred | TR-11. |
| TR-12-A | TR-12 | Reviewed multi-city policies integrate stations/orders/routes with physical regional production/exchange. | Implemented | Eight reviewed policies use the existing physical city inventories, routes, reports, station/order commands, and regional executor; abstract cities remain visiting-only. One assembled expanded-catalog station/order/route viewport journey remains unclaimed; see `TR-12.md`. |
| TR-12-B | TR-12 | Reversed discovery fingerprints match; Lübeck never enters automatic regional exchange. | Verified | `Hansa.Integration.Authoring.RegionalProduction.Catalog` and `ScaleProfile` pass; every observed automatic shipment rejects Lübeck at both ends. See `TR-12.md`. |
| TR-12-C | TR-12 | Profiled map/list/save/tick performance passes and each city advertises truthful capabilities. | Implemented | Thirty-city projection, bounded 48/20 materialization, search/filter, save/tick measurements, truthful capability assertions, and responsive semantic input pass. Shipping percentiles and real assembled viewport UAT remain unclaimed; see `TR-12.md`. |
| TR-13-A | TR-13 | Suspension, revocation, closure, recovery, and stranded cargo contracts preserve assets explicitly. | Verified | Seven explicit operating states, safe close/finalize, outbound-only recovery, route missed receipts, upkeep arrears, native recovery UI, and focused station automation; see `TR-13.md`. |
| TR-13-B | TR-13 | Every introduced save/catalog/fingerprint version migrates or fails safely and idempotently. | Verified | Genuine conditional layouts for formats 1–20 pass dry-run/apply/resave/repeat checks; checked-in v1/v2/v14 fixtures also pass. Loader gaps for 10/23, 11/24 and 19/31 were repaired; see `TR-13.md`. |
| TR-13-C | TR-13 | Conservation, command history, integer/tick boundaries, corruption, and missing-definition diagnostics pass. | Implemented | Candidate-ledger recovery, exact integer upkeep, duplicate/rejection rollback, corrupt archive atomicity and actionable policy/site/plot/capability diagnostics pass focused tests. Packaged real-viewport recovery UAT remains TR-14. |
| TR-14-A | TR-14 | All ten complete player journeys pass through ordinary packaged gameplay with alternative/recovery runs. | Blocked | Command-path suites pass, but no ordinary packaged ten-journey playthrough was completed; Windows capture was blocked by the ACL helper. See `TR-14.md`, TR14-UAT-01. |
| TR-14-B | TR-14 | Accessibility, localization, stale/unknown, input, viewport, performance, and soak evidence pass. | Blocked | Component accessibility, localization, 14 native 1280×720/1920×1080 bundles, and a 30-city micro-profile pass. Full rendered profile/input review and Shipping soak percentiles remain blocked; see TR14-UAT-02/04/05. |
| TR-14-C | TR-14 | Clean build/cook/package/launch passes and Shipping contains no forbidden development surface. | Blocked | Development game, Shipping target scan, cook and expanded cooked-content scan pass. Final IoStore audit and packaged launch are absent (`FinalIoStoreContainerAudited=false`); see TR14-UAT-03. |
| TR-14-D | TR-14 | Final conservation and requirement-to-evidence audit has no mandatory blocked row. | Blocked | Focused conservation/determinism tests pass, but mandatory earlier rows and TR-14-A/B/C remain blocked or deferred. Release status stays blocked. |

## Baseline version pins

| Contract | Inspected value |
| --- | ---: |
| Save envelope | 19 |
| Determinism fingerprint | 31 |
| Gameplay command schema | 13 |
| Multiplayer client intent schema | 7 |
| Simulation version | 1 |
| System pipeline | 1 |
| Accepted economic catalog | 33 / `92ABF14E33AA3606` |

## TR-05 implementation update

Current implementation pins are save **15**, fingerprint **28**, gameplay command **11**, client intent **5**, simulation/pipeline **1/1** and accepted catalog **v34 / D18AC831ED9C7710**. The earlier baseline table is historical. [TR-05](TR-05.md) records exact evidence, semantics, additive-default policy compatibility, and remaining acceptance gaps.

## TR-06 implementation update

Save **16** introduces explicit station/home action kinds while preserving all legacy 0/1 route semantics and historical fingerprints. Fingerprint **28**, command **11**, client intent **5**, simulation/pipeline **1/1**, catalog **v34 / D18AC831ED9C7710** remain unchanged. Focused integration, legacy route, migration, native trade-map, authoring and Shipping executable/receipt checks pass. Full rendered and remote-client acceptance remains blocked; see [TR-06](TR-06.md) and its hashed evidence manifest.

## TR-07 implementation update

Save **17**, fingerprint **29**, gameplay command **12**, and client intent **6** add event-attributed progression and the Merchant Office review/fund/construction lifecycle. Catalog version remains **v34**; the additive authored fields compile through the existing stage and city-policy definitions. See [TR-07](TR-07.md). Real viewport UAT is not claimed by the generated design references.

## TR-08 implementation update

Save **18**, fingerprint **30**, gameplay command **13**, and client intent **7** add exclusive Merchant Office specializations, price-limited orders, authoritative storage/order/route-handling effects, and native compact/wide comparison controls. Catalog version remains **v34**; the additive authored branch graph compiles through the existing city-policy definitions. See [TR-08](TR-08.md) and its evidence manifest.

## TR-13 implementation update

Save **21** and fingerprint **33** add explicit recoverable station interruption state and exact upkeep arrears. Gameplay command schema remains **14**, client intent **7**, and simulation/pipeline **1/1** because recovery uses existing authoritative Fund, Close, order, and route commands. Genuine historical-layout coverage now spans every format **1–20**, including repaired admission for **10/23**, **11/24**, and **19/31**. See [TR-13](TR-13.md).

## TR-14 release-audit update

The focused trade-presence, regional scale, accessibility, localization, rendered native-screen, Development-game, Shipping-target and Shipping-cook gates pass on 2026-09-21. The rendered UAT fixture was repaired to bind a ready scenario before acknowledging its briefing. Release remains **Blocked** because ordinary packaged journeys, full rendered profile/input review, Shipping soak percentiles, final IoStore audit/launch, and inherited mandatory rows are incomplete. See [TR-14](TR-14.md) and `TR-14/evidence.json`.

