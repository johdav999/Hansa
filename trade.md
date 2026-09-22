# Hansa trade, foreign presence, and city-control progression

## 1. Purpose and status

This document defines the intended player-facing trade experience in *Hansa*, from the regional trade map and route creator to the establishment of a permanent commercial presence in foreign cities. It combines the useful interaction principles seen in Anno-style route planning with Hansa's own market simulation, physical cargo, Hanseatic identity, and long-term city-control progression.

The core direction was explicitly requested by the user on 2026-09-20:

- the starting town is under full player control and supports unrestricted construction within normal gameplay rules;
- expansion into an existing foreign city begins through trade rather than immediate ownership;
- the player first trades as a visiting merchant, then establishes a trade station;
- continued business, investment, reliability, and local standing expand the player's presence;
- stronger presence improves trade and unlocks progressively broader construction and operational control;
- full city control is an exceptional final state, not the default result of opening one route.

This file distinguishes three kinds of statement:

1. **Current implemented/MVP behavior** — authoritative behavior already described in [Docs/MVP.md](Docs/MVP.md), [Docs/Development/TradeRoutes.md](Docs/Development/TradeRoutes.md), [Docs/Development/Market.md](Docs/Development/Market.md), and [Docs/Development/RegionalProductionEconomy.md](Docs/Development/RegionalProductionEconomy.md).
2. **Approved long-term direction** — trade grows into foreign presence and greater city control through trade stations and later institutions.
3. **Illustrative future design** — tier names, thresholds, costs, privilege branches, and exact balance values remain subject to implementation design, authoring, playtesting, and explicit promotion.

The existing MVP remains Lübeck and Rostock: Lübeck is the fully buildable home city; Rostock is rendered, prebuilt, inspectable, and tradeable but not player-buildable. The full foreign-presence ladder is a post-MVP expansion unless the MVP scope and acceptance gates are deliberately revised.

## 2. Design goals

Trade should accomplish more than moving goods between inventories. It should be the principal way a merchant house enters a foreign city, becomes useful to it, earns trust, acquires property, and eventually gains economic or civic authority.

The system should let the player answer:

1. What can I trade with this city now?
2. What is preventing a larger or more reliable operation?
3. What would a trade station improve?
4. What must I accomplish to expand my presence?
5. Which construction and commercial rights have I earned?
6. How is my activity affecting local stock, prices, production, and demand?

The experience should feel:

- commercial rather than territorial at first;
- based on physical goods, ships, storage, money, and travel time;
- responsive to the actual needs and production of each city;
- gradual, legible, and earned rather than unlocked by an unexplained experience bar;
- historically grounded in merchant offices, factors, warehouses, leased plots, guild standing, and negotiated privileges;
- strategically varied, so not every foreign city must become a second fully controlled settlement;
- scalable from the Lübeck–Rostock MVP to a larger Baltic and North Sea network.

## 3. Control model: home city versus foreign city

### 3.1 Home or founded city

The starting city is under direct player authority. The player may construct, demolish, organize production, manage roads and logistics, operate the market, and establish routes according to ordinary game rules.

A newly founded or explicitly chartered player settlement may eventually use the same model.

### 3.2 Existing foreign city

An existing foreign city remains an autonomous economic actor. The player initially has no construction rights and does not own its stock, production, workforce, streets, harbor, or policies.

The player gains specific rights in stages:

- access to public trade;
- permission to maintain a resident factor;
- leased storage and waterfront property;
- expanded merchant facilities;
- selected commercial or industrial construction rights;
- broader district and logistics authority;
- exceptional civic or governing authority where the scenario permits it.

Control is therefore permission-scoped rather than binary. A player may control a warehouse and private quay in Rostock while the rest of Rostock continues to simulate independently.

### 3.3 Four dimensions of presence

Foreign-city progression should be expressed through separate but related dimensions:

| Dimension | Examples |
| --- | --- |
| Commercial access | Goods, quantities, prices, reports, standing orders, fees |
| Physical presence | Trade station, warehouse capacity, quay access, leased plots |
| Operational control | Local carts, handling priority, route buffering, permitted workshops |
| Civic authority | Privileges, projects, district rules, taxation or governance where applicable |

Advancing one dimension does not automatically grant every ability in the others. This permits cities and scenarios to have distinct relationships with the player.

## 4. Core trade loop

The long-term player loop is:

```text
Discover city
    → inspect known market information
    → trade manually
    → build a reliable commercial relationship
    → establish a trade station
    → store and trade goods locally
    → automate sea routes
    → expand the merchant office
    → earn privileges and leased construction space
    → develop a merchant quarter or specialized branch
    → gain wider economic or civic control where permitted
```

The player should be able to stop at any stable stage. A distant source of salt may only need a small trade station. A strategically important regional center may justify a merchant quarter. A newly founded town may eventually become fully player-controlled.

## 5. Trade map and route GUI

### 5.1 Anno-derived interaction principle

The useful idea taken from the supplied Anno 1800 reference is the interaction structure, not its distinctive artwork or exact layout:

```text
Assigned ship or convoy
    + ordered city stops
    + cargo instructions at each stop
    + a geographic map
    + a repeating route schedule
```

Hansa must retain an original interface using its established navy, linen, ink, oak, and brass component system. It must not copy Anno's islands, cargo-slot grid, typography, ornaments, icons, colors, or branding.

### 5.2 Hansa screen layout

The regional trade workspace follows [Docs/UIDesignBrief.md](Docs/UIDesignBrief.md):

- **Top:** continuous Hansa status bar, screen title, map mode, time, season, speed, and close/back controls.
- **Left:** route and fleet list, foreign-presence filters, relevant alerts, and creation action.
- **Center:** ink-and-watercolor regional map with cities, ships, routes, reports, and selectable overlays.
- **Right:** selected city, trade station, ship, or route inspector.
- **Bottom:** ordered stop schedule, cargo manifest, journey progress, expected arrivals, and route preview.

The map remains visually dominant. Dynamic text, prices, quantities, route lines, status, controls, and focus are native UMG/Slate elements. Generated full-screen images are references only.

### 5.3 Route creation flow

1. Open the trade map.
2. Create a draft route.
3. Select an eligible player-owned ship.
4. Add cities as ordered stops.
5. Add load or unload instructions for known goods.
6. Set quantity caps and minimum source reserves.
7. Review reachability, capacity, reserve danger, destination capacity, travel time, upkeep, and winter delay.
8. Resolve validation errors.
9. Name and activate the route.
10. Monitor authoritative departure, travel, arrival, loading, and unloading.

Closing and reopening should retain a draft. Discarding a draft must leave the simulation unchanged. Invalid commands must state both cause and remedy.

### 5.4 Route and cargo presentation

The player should see a readable commodity flow rather than having to interpret repeated identical cargo slots:

```text
Bread    Lübeck: load up to 20 t, keep 40 t reserve
         → Cog Adler
         → Rostock: unload up to 20 t
```

The interface may show physical holds and capacity separately. Cargo actions remain connected visually across stops, but goods are primarily identified by icon and label rather than color.

### 5.5 Map modes

The scalable post-MVP map may offer:

- routes and fleet state;
- selected-good stock and price reports;
- supply, demand, reserve, and incoming cargo;
- foreign presence and available expansion;
- report age and uncertainty;
- sea, river, and land access when those route modes become player-facing;
- risk, season, diplomacy, or privileges only after the corresponding systems exist.

Unknown information is labeled unknown. Stale information remains visible with its report age and dashed or hatched treatment; it is never silently presented as current or as zero.

## 6. Current deterministic transport behavior

The implemented transport foundation is intentionally narrower than the complete commercial vision.

### 6.1 Vehicles and routes

- A vehicle has an authoritative cargo inventory, capacity, speed/travel duration, upkeep, owner, location, and route state.
- A route is an ordered cyclic list of stops.
- Each stop has ordered cargo actions.
- Current MVP actions are unconditional load and unload instructions with quantity caps and minimum source reserves.
- Loads are bounded by requested quantity, unreserved source stock above the minimum reserve, and remaining vehicle capacity.
- Unloads are bounded by carried cargo and destination capacity.
- Partial and missed transfers are explicit outcomes.
- Travel, upkeep, arrival, transfer, cancellation, and cargo remain deterministic and saved.

### 6.2 Important current boundary

Saved route action values 0/1 retain their legacy hybrid semantics for compatibility. TR-06 adds explicit station and home-city transfer actions alongside them. At a compiled city marked `bMarketOnly`, the current executor performs a priced purchase or sale; at Lübeck it remains an unpriced inventory transfer. TR-03 deliberately leaves this saved and hashed behavior unchanged and keeps it under `Hansa.Simulation.Trade.ForeignRouteSettlementCharacterization`.

New direct visiting commerce does not overload that route action. It uses the explicit `FHansaSpotTradeCommand`, authoritative price review, physical inventory movement, money settlement, receipt, and typed outcome event described below. TR-06 preserves that legacy behavior while station and home transfer actions are always unpriced. Station actions resolve the active station for the route owner and stop city, revalidate RouteAccess/LocalStorage, and preserve ledger reservations and live sale-order reserves. Movement runs before factor market updates. See Docs/Development/TradePresence/TR-06.md for the save-16 contract and acceptance limits.

## 7. Direct foreign trade before a station

A player who knows a foreign city but has no permanent presence may trade as a visiting merchant if the city permits public access.

Typical limitations are:

- only current public-market transactions;
- small or policy-bounded quantities;
- no local player-owned warehouse;
- no unattended buying or selling;
- no minimum-reserve buffer held on the player's behalf;
- slower harbor handling or lower priority;
- public fees where an implemented fee system exists;
- market information limited by discovery, report age, and local access;
- no construction rights.

The ship and its captain must remain present while a direct quay transaction is executed. Actual quantity and price depend on authoritative city stock, demand, money, ship capacity, and market rules.

Basic public trade should remain useful. The absence of a station must not make first contact impossible or trap the player in a progression dead end.

### 7.1 Implemented visiting spot-trade contract

An owned Cog physically berthed at an accessible foreign public market may buy or sell a visible good through the native market inspector. The estimate shows known report age, current known price and stock, requested quantity, projected cargo/cash effect, and explicit uncertainty. Confirmation revalidates ownership, berth and route state, city policy and capability, price review, stock/capacity, cargo, and money against current authoritative state.

The current city price is sampled at execution. Existing transaction friction is applied with half-away-from-zero unit-price rounding; buys settle with a ceiling and sales toward zero. The executable quantity is bounded deterministically, so stock, cargo, money, or capacity constraints produce a typed partial or missed result rather than fabricated goods or debt. Inventory movement, money, receipt, and typed event commit atomically from a candidate state; rejection or failure leaves authoritative state unchanged. The normal market system observes the new stock and owns later price changes.

Exact versioning, rollback/event order, UI semantics, generated visual references, and executable evidence are recorded in [Docs/Development/TradePresence/TR-03.md](Docs/Development/TradePresence/TR-03.md).

## 8. Why a trade station changes trade

A trade station, historically analogous to a small Kontor or permanent factor's office, creates a durable interface between the player's shipping network and the foreign city's autonomous market.

Its most important systemic function is to decouple the ship's sailing schedule from the exact moment at which the market can buy or sell.

### 8.1 Without a station

1. The ship arrives.
2. The city checks current public stock, demand, access, and price.
3. The ship trades immediately or waits according to the permitted direct-trade rules.
4. Missing supply, insufficient demand, lack of money, or capacity may produce a partial or missed transaction.
5. The ship leaves with whatever was actually obtained or sold.

### 8.2 With a station

1. A resident factor observes the market using the information access the station has earned.
2. Authorized orders buy goods over time using player money.
3. Purchased goods enter the station's physical local inventory.
4. Imported goods may be unloaded into that inventory.
5. Authorized sales release goods to the city market over time.
6. The player's ship arrives and loads prepared cargo from the station or unloads cargo into it.
7. The ship can depart without waiting for every market transaction to occur at berth.

The station therefore improves reliability, information, buffering, and throughput without creating goods or guaranteeing profit.

### 8.3 Initial trade-station capabilities

The first functional station should normally provide:

- a small player-owned local inventory;
- one resident factor;
- current or improved market reports for authorized goods;
- route loading and unloading through station stock;
- minimum station reserves;
- basic recurring purchase and sale policies when that system is implemented;
- a small leased harbor plot;
- station upkeep and local operating costs;
- an inspector explaining stock, orders, rights, costs, and blockers.

## 9. Foreign-presence progression

The following ladder expresses the approved direction. Names and exact thresholds are illustrative.

| Stage | Presence | Trade capabilities | Construction and control |
| --- | --- | --- | --- |
| 0 | Known or visiting merchant | Inspect known reports; perform permitted spot trades | None |
| 1 | Licensed trade contact | Add city to simple routes; improved reports; higher transaction allowance | None |
| 2 | Trade station / small Kontor | Local storage; resident factor; route buffering; basic recurring orders | One small leased harbor plot; station and basic warehouse elements |
| 3 | Merchant office | More goods, larger storage, better reports, faster handling, multiple routes or order slots | Expanded office, warehouse, quay, and approved logistics facilities |
| 4 | Merchant quarter | Local distribution, specialized branch benefits, permitted investments and workshops | Several leased plots or a bounded commercial district |
| 5 | Privileged merchant house | Preferential access, city projects, broader permitted industries and logistics authority | Selected civic/commercial projects and wider district rights |
| 6 | Chartered or governing authority | City-level policies and broad economic control where the scenario allows | Nearly full or full construction authority |

Stage 6 is exceptional. Existing historical cities should often remain autonomous even when the player's commercial presence is powerful. Full authority is more appropriate for a player-founded settlement, a special charter, or a scenario outcome than for routine trade volume.

## 10. How presence improves trade

Presence unlocks capabilities rather than applying one opaque percentage bonus.

| Improvement | Visiting | Trade station | Merchant office | Merchant quarter or privilege |
| --- | --- | --- | --- | --- |
| Market reports | Limited or stale | Regular local reports | Faster, broader reports | Best lawful information available |
| Local player storage | None | Small | Medium/large | Specialized and distributed |
| Automatic purchases/sales | None | Basic | Multiple standing orders | Advanced policies when implemented |
| Route buffering | None | One station inventory | Larger/multiple flows | Network-scale coordination |
| Handling speed | Public berth | Station handling | Improved quay/crew | Priority or private facilities |
| Goods access | Public selection | Authorized station goods | Broader licensed selection | Specialized or privileged goods |
| Local construction | None | Station plot | Commercial/logistics buildings | Bounded district and permitted industries |
| Local distribution | None | Station only | Market connection | Carts, warehouses, selected city links |
| Political influence | None | Reputation visibility | Formal petitions | Privileges/projects where implemented |

Any fee reduction, berth priority, tariff, contract, or political privilege must be backed by an implemented authoritative system. The UI must not promise mechanics that only exist as flavor text.

## 11. Progression requirements

Foreign presence should grow through demonstrated commercial value, not a generic unexplained experience bar.

Possible authored requirements include:

- cumulative lawful trade volume;
- number or value of completed deliveries;
- reliably supplying goods the city actually needs;
- maintaining a route for a minimum time without repeated defaults;
- positive local reputation or trust;
- paying construction and establishment costs;
- employing local labor;
- maintaining station upkeep and solvency;
- contributing to a harbor or civic project;
- completing a city request or council decision after those systems exist;
- possessing prerequisite research or house standing.

Every requirement must be inspectable. A locked upgrade should say exactly what remains:

```text
Expand to Merchant office

Trade delivered: 128 / 200 t
Reliable operation: 8 / 12 months
City standing: Trusted ✓
Required investment: 800 marks, 20 timber, 8 tools
```

Illustrative thresholds do not become balance requirements until authored, tested, and approved.

## 12. Specialization and strategic choice

From the merchant-office stage onward, presence may branch instead of advancing identically in every city.

Potential branches include:

- **Warehouse branch:** greater storage, reserve control, and route throughput.
- **Market branch:** better reports, more orders, and improved transaction access.
- **Harbor branch:** faster loading, private berths, repair or ship services when implemented.
- **Industrial branch:** permission to invest in selected local production stages.
- **Civic branch:** local projects, trust, and future privilege access.

A branch must grant concrete capabilities and opportunity costs. It must not be a collection of small percentage modifiers with no visible effect on play.

The currently authored merchant-office choice is one exclusive `MerchantOfficePrimary` group:

- **Warehouse:** 90,000 pfennig, 8,000 planks, and 1,000 tools; +75,000 milli-units of station storage.
- **Market:** 75,000 pfennig, 6,000 planks, and 2,000 tools; +4 standing-order slots and reviewed price-limited orders.
- **Harbor:** 110,000 pfennig, 10,000 planks, and 3,000 tools; +50,000 milli-units of station handling capacity per physical route operation.

Price limits use the friction-adjusted settlement price. Purchases execute at or below an inclusive ceiling; sales execute at or above an inclusive floor. The reviewed market tick and price must still match when the command is accepted. Respec returns 25% of the replaced branch's money investment, never its goods, and then charges the replacement in one atomic transaction. Closing the station clears the specialization.

City policy may prohibit some branches. For example, a city may welcome a warehouse but refuse foreign production, while a smaller port may offer industrial land to attract investment.

## 13. Construction in foreign cities

Foreign construction is plot- and permission-based.

- The player may construct only inside explicitly leased or granted areas.
- Available building categories come from the current presence stage and city policy.
- The rest of the city remains non-buildable and visibly outside player authority.
- Local roads, harbor connections, workforce, inputs, and services remain real requirements.
- A permitted workshop consumes actual inputs and deposits actual outputs into a physical inventory.
- Construction never grants ownership of the city's existing buildings or market stock.
- Local construction actions use the same authoritative command, validation, preview, accessibility, save, and automation contracts as construction in Lübeck.

When the player enters build mode in a foreign city, the world should clearly distinguish:

- player-owned plots;
- leased but unused plots;
- plots available for the next presence stage;
- prohibited city land;
- construction permitted with a warning;
- invalid construction with cause and remedy.

## 14. City autonomy and safeguards

The foreign-presence system must preserve an autonomous local economy.

- A station does not create goods, money, demand, or price information.
- Station purchases withdraw real goods from the city's available market stock and spend player money.
- Station sales add real goods to the appropriate market inventory and receive only the authoritative proceeds.
- Large purchases and sales affect supply, demand, and prices through the city market.
- Station inventory has finite capacity and explicit reservations.
- Goods do not teleport between station, ship, market, and workshop inventories.
- Ships still require capacity, travel time, upkeep, arrival, and handling.
- Orders may execute partially or not at all.
- Unknown or stale information must not become perfect forecasting.
- Minimum reserves protect owned source stock but do not reserve another city's public stock without an explicit accepted mechanism.
- A player cannot build outside granted plots or use local workforce without the relevant rights.
- AI merchant houses use the same economic commands, money, inventories, access rules, and progression requirements unless a scenario explicitly authors an asymmetry.

These safeguards prevent a trade station from becoming a magical global warehouse or passive resource faucet.

## 15. Loss, suspension, and recovery

Presence should not disappear without explanation, but it may become constrained.

Possible states include:

- active;
- upgrade available;
- construction in progress;
- underfunded;
- storage blocked;
- order suspended;
- rights temporarily suspended;
- station closed voluntarily;
- privilege revoked after an implemented political or legal event;
- inaccessible because of war or route conditions after those systems exist.

A suspended station retains authoritative assets and inventory unless a specific rule states otherwise. The UI explains what is frozen, what still costs upkeep, what cargo can be recovered, and how normal operation can resume.

## 16. UI representation of foreign presence

### 16.1 City markers

Each city marker should communicate presence through shape and iconography, not color alone:

- hollow marker: known market only;
- small license tab: trade contact;
- single brass ring with station emblem: trade station;
- double ring: merchant office;
- quartered seal: merchant quarter;
- house seal or charter device: governing authority.

Selection, report age, shortage, route connection, and presence are separate layers so one state does not overwrite another.

### 16.2 City-presence inspector

Selecting a foreign city should show:

```text
ROSTOCK

Presence: Trade station
Standing: Trusted
Market report: Current
Station storage: 28 / 50 t
Active routes: 1

Available now
✓ Automatic route transfers
✓ Local station inventory
✓ Minimum station reserves

Next: Merchant office
Trade another 72 t
Maintain reliable operation
Invest required money and materials

[Inspect station]  [Expand presence]
```

All values must come from authoritative state. Unknown values show an explicit unavailable state.

### 16.3 Route editor integration

Access-dependent actions remain visible when useful but are disabled with a reason:

> Standing purchase orders require a Merchant office in Rostock.

The editor should distinguish:

- direct trade with a city market;
- inventory transfer between a ship and a player-owned station;
- station purchase or sale orders;
- transfers between station storage and permitted local buildings.

These must not be represented as the same command if they have different ownership, money, or price effects.

### 16.4 Construction transition

The city inspector provides an explicit action to enter the permitted foreign construction area. Exiting construction returns to the same selected city and presence context. Locked building categories explain the required presence stage or privilege.

## 17. Interaction states and accessibility

Required states include:

- unknown city;
- discovered city;
- public trade available;
- access denied;
- station available to establish;
- establishment unaffordable;
- station under construction;
- station active;
- upgrade available;
- presence suspended;
- stale report;
- route valid, warning, or invalid;
- station storage empty, partial, full, or blocked;
- default, hover, pressed, selected, disabled, loading, keyboard/controller focus, warning, and error for every applicable control.

Status is communicated with icon, label, shape, or line pattern in addition to color. Body text and essential icons follow the contrast requirements in [Docs/UIDesignBrief.md](Docs/UIDesignBrief.md). Mouse, keyboard, and controller flows must be complete, and no critical progression action may require drag input alone.

## 18. MVP and delivery sequence

### 18.1 Current MVP

The current MVP requires:

- fully buildable Lübeck;
- prebuilt, rendered, inspectable, tradeable, non-buildable Rostock;
- one player Cog;
- a production route creator;
- ordered stops;
- load, unload, quantity cap, and minimum reserve;
- travel time, capacity, upkeep, approximate preview, and deterministic winter delay;
- visible authoritative cargo departure, travel, arrival, and unloading;
- real market and inventory response;
- no piracy, combat, convoy, insurance, conditional price rules, contracts, tariffs, or full diplomacy system.

To preserve current acceptance, Rostock may be treated as already having the minimum commercial access needed by the MVP route. A visible trade-station establishment step can be added to the MVP only through a deliberate revision of [Docs/MVP.md](Docs/MVP.md), its scenario balance, editor coverage, save migration, automation, and acceptance evidence.

### 18.2 Recommended progression increments

1. **Transport foundation:** existing deterministic load/unload routes and visible Cog.
2. **Trade-station foundation:** station identity, inventory, establishment cost, one leased plot, inspector, save state, and route transfer integration.
3. **Explicit market transactions:** station purchase and sale orders with money and price effects.
4. **Merchant-office progression:** capacity, order, report, route, and handling upgrades.
5. **Foreign construction permissions:** leased plots and selected logistics/commercial buildings.
6. **Merchant-quarter specializations:** warehouse, market, harbor, industrial, or civic branches.
7. **Privileges and civic authority:** only after contracts, politics, and city-policy systems exist.

Each increment must deliver game, editor, validation, migration, tests, UI semantics, save/load, and real-viewport evidence together.

## 19. Authoritative data and editor parity

When implemented, foreign presence should use stable Hansa identities and authored definitions rather than UI strings or asset filenames.

Likely concepts include:

- house/player identity;
- city identity;
- foreign-presence state;
- presence-stage definition;
- city access policy;
- permission set;
- leased plot allocation;
- station and office identity;
- station inventory and reservations;
- factor/order identity;
- progress requirement and contribution history;
- active, suspended, and revoked state;
- authored upgrade costs and prerequisites.

Commands should be closed, typed, validated, and deterministic. Candidate actions include establish station, upgrade presence, choose specialization, create or cancel an order, allocate a leased plot, construct a permitted building, suspend or close an office, and recover cargo.

Any gameplay-model change must update its Authoring Studio schema, metadata, validation, migration, impact analysis, tests, projections, semantic UI, save fingerprint, and applicable generation/import workflow in the same implementation stream. Runtime modules must remain independent of editor and provider-integration code.

## 20. Verification expectations

An implemented foreign-presence feature is not complete because a mockup or headless test exists. Evidence should demonstrate:

- establishing a station through ordinary player controls;
- money and materials conserved;
- the station's physical inventory before and after transactions;
- route cargo correlated with the visible Cog;
- city market stock and price effects;
- partial and failed orders with clear causes;
- leased construction accepted only in authorized plots;
- unauthorized construction rejected without state mutation;
- progression requirements and unlocks matching authoritative projections;
- save/load preservation of presence, inventory, orders, plots, permissions, and routes;
- deterministic replay and order-independent data discovery;
- AI use of normal rules where applicable;
- native 1280 × 720 and 1920 × 1080 viewport captures;
- large-text, high-contrast, reduced-motion, keyboard, mouse, and controller coverage;
- Shipping exclusion of editor, automation, staging, provider, and test-only surfaces.

## 21. Visual references

The current composed trade references are:

- [Route creator — valid draft](Docs/Images/UI/TradeMap/trade-map--route-creator--valid-draft--1672x941--v1.png)
- [Active route — sailing](Docs/Images/UI/TradeMap/trade-map--active-route--sailing--1672x941--v1.png)
- [Reference-set component inventory](Docs/Images/UI/TradeMap/trade-map--reference-set--v1.md)

These images establish visual hierarchy and style only. Their text, values, cartography, controls, and interaction are not authoritative implementation. The screen must be assembled from native widgets and approved individual assets.

## 22. Summary

Hansa's trade system should grow from simple physical shipping into a broader commercial-presence game:

```text
Trade creates familiarity.
Reliable commerce creates trust.
Trust permits a permanent station.
The station makes routes and market activity more reliable.
Investment expands the merchant office.
Economic value earns construction rights and privileges.
Only exceptional authority turns foreign presence into full city control.
```

This structure connects the regional trade map, physical cargo, market simulation, foreign-city expansion, and the player's gradual rise from merchant to city power without making every new city instantly behave like the fully controlled starting town.
