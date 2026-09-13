# Hansa — UI and GUI Design Brief

## 1. Purpose

This brief defines the interface direction for *Hansa*, an Anno-inspired city-builder and trading game set in northern Europe during the Hanseatic era. It covers visual style, color, typography, screen hierarchy, interaction, usability, accessibility, motion, audio feedback, and Unreal Engine implementation constraints.

The interface must make a deep economy feel understandable. A player should be able to answer three questions quickly:

1. **What is happening?**
2. **Why is it happening?**
3. **What can I do about it?**

The UI should feel historically grounded without behaving like a historical document. Decorative treatment supports atmosphere; modern interaction standards protect clarity.

All UI and image work must also follow the component-generation and no-resampling rules in [UIAssetWorkflow.md](UIAssetWorkflow.md).

## 2. Experience goals

### Primary goals

- Keep the 3D city visible and inviting during normal play.
- Make production chains, population needs, and trade routes readable at a glance.
- Explain the causes of shortages, price changes, congestion, and unrest.
- Support both approachable default controls and advanced economic automation.
- Make city, market, and world-map modes feel like parts of one system.
- Remain efficient at 30–50 cities, 50–70 goods, and 2–8 players.
- Work for mouse/keyboard first, with complete controller navigation planned from the start.

### Desired emotional qualities

- prosperous;
- industrious;
- maritime;
- civic;
- tactile;
- trustworthy;
- quietly prestigious rather than royal or militaristic.

### Avoid

- fantasy-medieval ornament;
- excessive parchment texture behind dense data;
- blackletter body text;
- glossy mobile-game panels;
- opaque resource icons with no labels or tooltips;
- alerts that state a problem without explaining its cause;
- full-screen menus for actions that should be handled contextually;
- red/green-only communication;
- copying the distinctive UI assets, layout, typography, or branding of any existing game.

## 3. Audience and interaction profile

### Core players

- city-builder players who enjoy optimizing production layouts;
- strategy players who want a simulated economy and meaningful logistics;
- multiplayer players who need rapid comparison and negotiation tools;
- history-oriented players who value authenticity but do not want archival complexity.

### Usage pattern

Players alternate between three scales:

1. **City scale:** build, inspect, solve logistics, and observe citizens.
2. **Regional scale:** manage multiple settlements, fleets, routes, and shortages.
3. **European scale:** compare markets, negotiate privileges, and plan long-distance trade.

The interface must preserve context when switching scale. A selected good, city, route, or house remains selected where meaningful.

## 4. Visual identity

### Design statement

**A merchant's counting house overlooking a busy Baltic harbor.**

The UI combines period-inspired materials with disciplined modern data design:

- oak frames and desks;
- linen and rag-paper surfaces;
- iron brackets and brass dividers;
- inked maps and ledger ruling;
- stamped wax seals for authority and signed agreements;
- red brick and dark painted timber as architectural accents;
- nautical chart lines for routes and geographic overlays.

Materials should be suggested through restrained borders, texture, and lighting—not simulated so literally that they reduce contrast or waste space.

### Shape language

- Mostly rectangular panels with 2–4 px corner rounding.
- Chamfered or clipped corners for important modal cards and house emblems.
- Thin ruled dividers inspired by ledgers.
- Circular seals reserved for signed, completed, official, or locked states.
- Route nodes and market markers use simple geometric silhouettes.
- Ornamental flourishes appear only in headers, empty states, and major achievements.

### Surface hierarchy

| Level | Treatment | Use |
| --- | --- | --- |
| World overlay | Transparent charcoal/navy scrim | Routes, ranges, build grid, heatmaps |
| Floating control | Dark painted timber/slate | HUD controls and tool palettes |
| Working panel | Warm linen/paper | Tables, inspectors, market and production data |
| Important decision | Light paper with brass/oxblood frame | Contracts, council votes, victory progress |
| Critical warning | Desaturated dark panel with amber/red edge | Bankruptcy, famine, attack, failed route |

## 5. Color system

### Core palette

| Token | Hex | Use |
| --- | --- | --- |
| Baltic Navy | `#152A35` | Primary dark surface, top bar, deep overlays |
| Harbor Slate | `#29424D` | Secondary dark surface, selected dark controls |
| Ink | `#202628` | Primary text on light surfaces |
| Muted Ink | `#596160` | Secondary text and inactive metadata |
| Linen | `#F2E9D8` | Primary light panel |
| Parchment | `#DFCFAF` | Secondary light panel, rows, cards |
| Oak | `#795137` | Frames and material accent |
| Brass | `#C19A52` | Focus, premium actions, progress, dividers |
| Hanseatic Brick | `#A44C3F` | Identity accent and important active states |
| Oxblood | `#762F32` | Destructive/critical accent |
| Prosperity Teal | `#35766F` | Healthy supply, completed route, positive state |
| Baltic Blue | `#397FA3` | Water, neutral route, information selection |
| Warning Amber | `#D09132` | Shortage risk, delay, attention |
| Frost Blue | `#9CC3CF` | Winter, ice, paused/cool state |
| Chalk | `#FAF7EF` | High-contrast text and icon on dark surfaces |

### Semantic application

- Positive: Prosperity Teal plus upward arrow or check.
- Informational: Baltic Blue plus information glyph.
- Warning: Warning Amber plus triangle or clock.
- Critical: Oxblood plus octagon/exclamation.
- Selected: Brass outline with a subtle light fill.
- Disabled: Muted Ink at reduced contrast, plus disabled cursor/tooltip explanation.
- Other players: assigned colors must pass contrast checks and also use emblem/pattern identity.

Color never communicates status alone. Every colored state receives an icon, label, shape, pattern, or direction marker.

### Economic chart colors

Charts use a stable semantic mapping:

- price: Brass;
- city stock: Baltic Blue;
- citizen demand: Hanseatic Brick;
- industrial demand: Oak;
- expected incoming supply: Prosperity Teal, dashed;
- desired reserve: Muted Ink, dotted;
- stale/estimated data: desaturated and hatched/dashed.

Do not use a rainbow palette for goods. Goods are identified by icon and text; color remains available for economic meaning.

### Contrast targets

- Body text: minimum WCAG 2.1 AA contrast of 4.5:1.
- Large text and essential icons: minimum 3:1.
- Never place small ink text over visibly mottled texture without a flat backing layer.
- Provide a high-contrast option that removes most material texture and strengthens outlines.

## 6. Typography

### Type roles

| Role | Direction | Notes |
| --- | --- | --- |
| Display/header | Humanist or transitional serif | Civic, historic, confident; use sparingly |
| Body/UI | Highly legible sans serif | Dense tables, controls, tooltips, localization |
| Numbers | Tabular lining numerals | Prices, inventory, time, production rates |
| Decorative | Restrained inscribed/blackletter influence | Logo or rare ceremonial heading only |

Suitable open-font directions to evaluate include Source Serif 4 for display and Atkinson Hyperlegible or Noto Sans for UI. Final font licensing and language coverage must be verified before inclusion.

### Scale at 1920×1080 reference resolution

| Token | Size | Use |
| --- | --- | --- |
| Display | 32–40 px | Major screen title or victory moment |
| H1 | 24–28 px | Panel/screen heading |
| H2 | 18–20 px | Section heading |
| Body | 15–17 px | Normal labels and descriptions |
| Data | 14–16 px | Tables and compact economic data |
| Caption | 12–14 px | Metadata; never essential at smallest size |

Text should remain readable at the smallest supported UI scale. Use tabular numerals and align numeric columns on the decimal/unit boundary.

### Writing style

- Prefer direct labels: “Grain reserve: 4 days,” not “A paucity of grain has arisen.”
- Use historical terminology only when it creates meaningful flavor; explain it on first use.
- Show units consistently: `24 t`, `8 / min`, `14 days`, `120 pfennig`.
- Use sentence case for controls and headings.
- Buttons begin with verbs: “Create route,” “Accept contract,” “Reserve stock.”
- Error messages include cause and remedy.

## 7. Spacing and layout system

### Grid

- 8 px base spacing unit.
- 4 px allowed for icon/text micro-spacing.
- Standard panel padding: 16 or 24 px.
- Major screen gutters: 24–32 px.
- Minimum pointer target: 40×40 px; preferred 44×44 px.
- Minimum controller focus target: 48×48 px.

### Resolution strategy

- Design reference: 1920×1080, 16:9.
- Support 16:10, 21:9, 32:9, and windowed modes.
- Anchor HUD clusters to safe-area edges; do not stretch data tables across ultrawide screens.
- Center critical modals within a bounded maximum width.
- Allow UI scale from at least 80% to 140%.
- Verify at 1280×720, 1920×1080, 2560×1440, and representative ultrawide.
- Respect platform safe zones and streamed/captured content margins.

### Panel behavior

- Right inspector width: 360–440 px at 1080p.
- Full management screen maximum content width: 1600 px, centered where appropriate.
- Drawers preserve the world view and close with Escape/right-click/back.
- Panels remember user-set width where it improves comparison.
- Opening a second related object uses a comparison split, not a pile of windows.

## 8. Main gameplay HUD

The default HUD leaves at least 70% of the center unobstructed.

```text
┌──────────────────────────────── TOP STATUS BAR ────────────────────────────────┐
│ House resources   City/region   Season/date + speed   Influence/research  Menu │
├───────────────┐                                              ┌────────────────┤
│ Objectives &  │                                              │ Contextual     │
│ alert stack   │              3D CITY / WORLD                 │ inspector      │
│ (collapsible) │                                              │ (when needed)  │
│               │                                              │                │
├───────────────┘                                              └────────────────┤
│ Minimap / overlays      SELECTION + BUILD TOOLBAR        Notifications/history│
└────────────────────────────────────────────────────────────────────────────────┘
```

### Top status bar

- House crest and current house/player color.
- Money and trend; hover opens cash-flow breakdown.
- Current workforce by tier, collapsed into shortage/surplus summary.
- Selected city/region breadcrumb.
- Season, date, and weather/route condition.
- Pause and speed controls at the top right, using distinct icons and explanatory tooltips.
- Influence, research, reputation, and victory progress as compact indicators.
- Multiplayer connection/host state and main menu at the far end.

The bar shows summaries, not every resource. Goods belong in storage, market, production, and pinned watch lists.

### Objectives and alert stack

- Maximum three expanded alerts; remaining alerts group by category.
- Severity, affected city/object, cause, and age are visible.
- Clicking an alert selects and frames the relevant object or opens the causal panel.
- Alerts can be snoozed, muted by category, or converted into pinned trackers.
- Repeated low-level production warnings aggregate instead of spamming.

### Bottom toolbar

- Context-sensitive selection actions appear above or beside the persistent build categories.
- Build categories: Roads, Residences, Production, Storage, Harbor, Civic, Decoration.
- Recently used and favorites are immediately accessible.
- Keyboard shortcuts appear in tooltips and can be rebound.
- Construction cost, workforce, footprint, and prerequisites appear before placement.

### Contextual inspector

The right inspector is the main detail surface for buildings, residences, ships, routes, and citizens. It follows a stable order:

1. identity and state;
2. most important result;
3. inputs/outputs or needs;
4. current problem and cause;
5. actions and automation;
6. historical details and flavor.

Common actions remain in consistent positions across object types.

## 9. Build mode

### Placement feedback

- Valid footprint: teal outline and subtle grid fill.
- Invalid footprint: oxblood outline, striped cells, and precise reason next to the cursor.
- Conditional/warning: amber outline, such as “valid but outside market range.”
- Road, shore, service, fertility, workforce, and logistics overlays can be toggled without leaving placement.
- Show the affected production/service radius and predicted logistics connection.
- Before confirmation, show construction cost, upkeep, workforce, and missing prerequisites.

### Interaction

- Left click/A confirms; right click/B cancels one level.
- Rotate, eyedropper, upgrade, move where allowed, and continuous placement are direct actions.
- Shift repeats the current building; Alt temporarily disables snapping where valid.
- Drag placement works for roads, walls, fields, and repeated residences.
- Demolition offers a short undo/grace period when multiplayer rules permit.

### Construction menu card

Each card includes:

- building icon and name;
- category and tier;
- construction cost;
- workforce and upkeep;
- compact input → output chain;
- locked/invalid reason;
- favorite and comparison action.

## 10. City and population interface

### City overview

Use a dashboard with four primary tabs:

1. **Population** — tiers, homes, needs, migration, affordability.
2. **Production** — chain balance, throughput, bottlenecks, workforce.
3. **Market** — stock, consumption, incoming supply, prices.
4. **Administration** — policies, privileges, taxes, civic works.

The header always displays population trend, treasury contribution, satisfaction, available workforce, staple reserve days, and current city alerts.

### Needs presentation

For each population tier show:

- number of residents and residences;
- workforce used/available;
- need icon and label;
- access, affordability, and reliability as separate states;
- current consumption per minute/day;
- reserve days;
- effect of fulfillment;
- button to reveal the supplying chain and route.

A need is never shown as one unexplained green/red bar.

### Production-chain view

- Horizontal left-to-right chain flow.
- Each node shows actual/nominal throughput and utilization.
- Edge labels show quantity per cycle and missing flow.
- Warehouse and transport delay appear as distinct bottlenecks.
- Selecting a node highlights matching buildings in the city.
- “Supportable residences” translates output into an understandable city-building metric.

## 11. Market interface

The market is the signature screen and must be powerful without resembling a modern stock-trading terminal.

### Layout

```text
┌ City + report timestamp ─ Search ─ Filters ─ Compare city ────────────────┐
│                                                                           │
│ Goods table (55–65%)                         Selected good (35–45%)       │
│ Icon / Good / Stock / Reserve / Demand       Price history chart          │
│ Local price / Trend / Incoming / Status      Supply-demand explanation    │
│                                              Consumers and producers      │
│                                              Buy/sell/route actions        │
├───────────────────────────────────────────────────────────────────────────┤
│ Pinned watch list / active orders / relevant routes                       │
└───────────────────────────────────────────────────────────────────────────┘
```

### Goods table

- Sort and filter by category, price trend, shortage, owned stock, route, or opportunity.
- Sticky header and virtualized rows.
- Icon is always paired with a text label at least once in the current context.
- Stock displays amount and reserve days.
- Price trend uses arrow + percentage/absolute change + sparkline.
- Stale reports show age and switch to dashed/hatched visual treatment.
- Selecting multiple cities enables side-by-side comparison without losing the chosen good.

### Price explanation

The selected-good panel answers:

- Base value.
- Current local price.
- Difference from recent average.
- Stock versus desired reserve.
- Citizen and industrial demand.
- Expected incoming shipments.
- Taxes, privileges, embargoes, and seasonal modifiers.
- Confidence/report age.

Example explanation:

> Grain is 28% above its 30-day average. The city holds four days of reserve, two bakeries are under-supplied, and the next confirmed shipment arrives in six days.

### Market actions

- Buy/sell now where the player's office permits.
- Create conditional order.
- Reserve minimum city/warehouse stock.
- Add good to a route.
- Create a new route from this opportunity.
- Pin price/stock alert.
- Compare nearby known markets.

## 12. European trade-map interface

### Map treatment

- Desaturated ink-and-watercolor geography over a dark Baltic/navy field.
- Coastlines and rivers remain readable beneath overlays.
- Cities use scalable markers with status rings.
- Sea routes use solid curves; land routes use dashed lines; river routes use double-edge or wave markers.
- Route thickness communicates capacity; animation communicates direction only when useful.
- Hazard, delay, and outdated-information states use icon and line pattern, not color alone.

### Layout

- Left: route/fleet list, filters, alerts.
- Center: European map with routes, cities, and selectable overlays.
- Right: selected route/city inspector.
- Bottom: route schedule/cargo manifest and expected arrivals.
- Top: scale breadcrumb, map mode, time/speed, and house summaries.

### Route editor

Two levels of complexity:

- **Simple:** stop, load, unload, minimum reserve.
- **Advanced:** quantity limits, price thresholds, conditional stop, seasonal behavior, substitutions, convoy, risk policy.

The route preview shows round-trip time, capacity utilization, upkeep, tolls, risk, expected profit range, and which city's reserve would be endangered.

## 13. Research, politics, and diplomacy

### Research tree

- Use six visually distinct branches arranged as a navigable chart, not an unbounded radial web.
- Locked nodes reveal prerequisites and strategic effect.
- Compare the next 2–3 candidate technologies.
- Clearly distinguish unlocks from efficiency bonuses.
- Research queue and estimated completion remain visible.
- Selecting a node can highlight affected buildings/routes in other screens.

### Hanseatic assembly and city politics

- Present major votes as formal paper dossiers with house seals and coalition positions.
- Show proposal, direct effects, supporters, opponents, undecided votes, deadline, and influence cost.
- Private promises and public votes have separate visual states.
- A decision recap explains which rule changed and when it takes effect.

### Contracts and diplomacy

- Contract cards prioritize parties, goods/service, quantity, destination, deadline, reward, penalty, and route feasibility.
- Player-authored offers preview whether all terms are enforceable by the game.
- Chat/ping tools link cities, goods, routes, contracts, and prices as interactive references.

## 14. Notifications and decision severity

| Severity | Presentation | Examples |
| --- | --- | --- |
| Ambient | Timeline only or subtle toast | Production completed, routine arrival |
| Notice | Small timed toast | Route delayed, building input low |
| Warning | Persistent alert with amber edge | Staple below reserve, workforce shortage |
| Critical | Strong alert and optional pause | Bankruptcy imminent, famine, city fire |
| Decision | Modal or anchored dossier | Council vote, peace offer, victory choice |

Modals are reserved for decisions that block time-sensitive progression, spend exceptional resources, affect other players, or are difficult to reverse. Routine errors remain contextual.

## 15. Tooltips and explanations

### Tooltip layers

- Short hover: name, current value, state, shortcut.
- Delayed expanded tooltip: cause, formula factors, trend, related object.
- Inspector link: opens the full causal panel without forcing the player to memorize the tooltip.

### Formula presentation

Do not expose raw code formulas by default. Show a human-readable factor stack:

```text
Bread output                         2.0 / min
Workforce shortage                  -15%
Road delivery delay                 -10%
Experienced bakers                   +5%
Actual output                        1.6 / min
```

Advanced settings may enable exact values and debug breakdowns.

## 16. Information age and uncertainty

Market information is a game mechanic and needs a consistent visual language:

- Live/current: solid line, normal saturation, open-eye/report icon.
- Recent report: small timestamp and slightly reduced saturation.
- Stale: dashed borders/lines, faded data, hourglass/courier icon.
- Estimated: value range and hatched fill.
- Unknown: explicit “No recent report,” never a misleading zero.

Every remote price shows the city and report date. When planning a route, expected profit appears as a range whose uncertainty grows with report age and travel time.

## 17. Multiplayer usability

- Every player is identified by color, emblem, and name.
- Team/shared property adds a clear ownership badge.
- Remote cursor/ping is optional and rate-limited.
- Contracts and negotiations can reference objects through clickable chips.
- Pause/speed policy and current votes are always visible.
- Destructive actions against shared assets require permission and clear ownership feedback.
- Reconnecting players see a concise “while you were away” timeline.
- Defeated/insolvent players receive recovery options, not a blank spectator UI.

## 18. Accessibility

### Visual

- Color-vision presets plus shape/pattern redundancy.
- High-contrast mode.
- UI scaling and large-text preset.
- Adjustable map/overlay line thickness.
- Reduced texture mode for data panels.
- Avoid essential flashing; provide photosensitivity-safe effects.
- Do not place text over moving 3D scenes without an opaque/blurred backing.

### Input

- Fully remappable keyboard and mouse.
- Complete controller focus navigation with visible focus ring.
- Adjustable edge-scroll speed and ability to disable edge scrolling.
- Toggle/hold options for camera rotation and overlays.
- Adjustable double-click and tooltip delays.
- Avoid drag-only operations; provide click/select alternatives.

### Cognitive and information accessibility

- Consistent position for common controls.
- Plain-language explanations and icon labels.
- Pause-on-critical-alert option in solo play.
- Beginner/advanced density modes rather than removing economic rules.
- Search across cities, goods, buildings, ships, routes, and research.
- Tutorial hints are dismissible and recoverable from help.

### Audio

- Separate UI, ambience, music, and notification volume.
- Captions/text equivalents for meaningful audio alerts.
- Distinct but non-startling sounds for confirmation, rejection, warning, and critical events.

## 19. Motion and feedback

- Standard panel transition: 120–180 ms.
- Major scale transition: 250–400 ms with direct camera continuity.
- Hover/focus feedback: under 100 ms.
- Do not animate continuously unless state or direction is changing.
- Respect reduced-motion setting: replace sliding/zooming with fades or immediate state changes.
- Successful construction uses a brief material/outline settle, not a large celebratory effect.
- Numeric changes pulse once and preserve the previous value long enough to understand direction.
- Invalid actions provide visual feedback and a concise reason; avoid punishing sound repetition.

## 20. Iconography and imagery

### Mandatory ImageGen use for every GUI image and icon

- Always use ImageGen to create or revise every image and every icon used in the GUI, including goods, resources, population tiers, navigation, status indicators, and speed controls.
- Generate each distinct image or icon individually. A composed screen mockup or a generic component reference does not satisfy this requirement for the individual images and icons it contains.
- Use the selected ImageGen artwork in the implemented GUI. Do not substitute manually drawn Slate/C++ shapes, improvised SVGs, font glyphs, emoji, stock icons, or placeholder symbols for the generated icon artwork.
- Use built-in ImageGen by default. Use an explicit API/CLI workflow only when the user authorizes it. If generation is unavailable, report the blocker instead of silently substituting another method.
- Reuse an existing approved ImageGen asset when appropriate; preserve its source and prompt record. Save every new selected asset in the repository with its individual prompt, native dimensions, generation mode, and revision notes.
- Inspect each image and icon at its intended native display size and verify the assembled GUI visually. Passing layout or behavior tests alone does not establish production visual quality.
- Continue to implement layout, dynamic text, values, tooltips, hit targets, focus states, and interactions natively in UMG/Slate. Native widget implementation does not exempt its images or icons from ImageGen generation.
- The no-resampling, transparency, style consistency, and asset-validation rules in this brief and UIAssetWorkflow.md still apply. This requirement supersedes earlier guidance permitting GUI icons to be replaced with manually drawn native or vector symbols unless the user explicitly authorizes an exception.

### Icon style

- Strong silhouette at 20–24 px.
- Slightly engraved/inked character at large sizes, flat at small sizes.
- Consistent three-quarter or profile view by category.
- Goods use literal objects: grain sheaf, hop cones, malt sack, empty barrel, beer vessel, fish, plank, tool.
- Beer-chain building cards use distinct literal silhouettes for the lumber camp, hop farm, malt house, cooperage, and brewery; their labels and native state treatments remain separate from the artwork.
- Systems use abstract symbols only when common: gear, route arrow, scales, hourglass.
- Never distinguish two goods only by color.

### Portraits and historical decoration

Portraits are useful for merchant houses, officials, guild leaders, and event characters. They should be period-grounded and restrained. Decorative illustrations may appear in loading screens, scenario introductions, empty states, and victory panels—not beneath operational data.

## 21. Audio identity for GUI

- Paper slide: opening dossiers and market reports.
- Soft wooden click: ordinary selection and placement.
- Coin/scale detail: confirmed commercial transaction.
- Wax seal press: signed contract or formal vote.
- Ship bell: important arrival or maritime alert.
- Low muted bell: critical city warning.

Sounds should be short, warm, and materially grounded. Repeated route/production events aggregate into one sound rather than creating an audio cascade.

## 22. Unreal Engine implementation guidance

### C++ responsibilities

- `UObject` presentation/view models expose stable UI-ready state.
- C++ presenters translate domain events and read models into widget updates.
- Numeric formatting, sorting, filtering, virtualization data, and causal breakdowns live in tested C++.
- Enhanced Input actions and mapping contexts express UI/world intent.
- Custom Slate widgets are appropriate for high-performance charts, large virtualized tables, and route timelines when UMG composition is insufficient.

### Blueprint responsibilities

- UMG widget composition and responsive layout.
- Style assets, animation, audio, and transition hooks.
- Blueprint children of C++ widget bases where designers need variation.
- Visual empty states, tutorials, event dossiers, and screen-specific presentation.

### Performance rules

- No Blueprint tick for normal widgets.
- No raw UMG bindings that evaluate every frame across large data sets.
- Update widgets through events/field changes and dirty view models.
- Virtualize market, fleet, building, and notification lists.
- Pool frequently created row widgets/tooltips where profiling supports it.
- Use invalidation strategically; do not wrap rapidly changing giant trees without measurement.
- Keep texture atlases and UI materials within defined memory budgets.

### Style implementation

Create centralized style tokens for:

- colors and semantic colors;
- typography and numeric styles;
- spacing and panel padding;
- borders, shadows, and focus rings;
- buttons, toggles, tabs, tables, cards, and tooltips;
- motion durations and curves;
- notification severity.

Widgets must consume tokens rather than embedding independent color and spacing values.

## 23. Usability validation plan

### Prototype tests

1. Can a new player find why bread is unavailable within 30 seconds?
2. Can the player create a grain route without opening help?
3. Can the player distinguish local price, known remote price, and stale report?
4. Can the player locate a production bottleneck and the affected residences?
5. Can the player compare two cities without losing the selected good?
6. Can the player recover from an invalid building placement?
7. Can controller users reach every visible action and return focus predictably?
8. Can color-blind users distinguish route/status categories without labels?

### Telemetry candidates

- time from alert to causal panel;
- route-creation cancellation/error rate;
- frequency of accidental export below reserve;
- screens opened before solving a shortage;
- tooltip dwell and advanced-breakdown usage;
- modal cancellation and confirmation rates;
- UI scale and accessibility setting adoption.

Telemetry informs iteration but does not replace observation and interviews.

## 24. Deliverable sequence

1. Component inventory, implementation classification, and state matrix for each screen.
2. Low-fidelity wireframes for main HUD, market, trade map, build mode, and research.
3. Clickable interaction prototype for shortage diagnosis and route creation.
4. Final color/type/style tokens and approved style-anchor image.
5. High-fidelity composed screen references plus separate references/assets for every reusable component.
6. Unreal UMG/Slate implementation of the vertical-slice screens using native text, data, controls, and layout.
7. Native-resolution asset inspection and accessibility, localization, controller, aspect-ratio, and performance validation.
8. Expanded screens for politics, contracts, multiplayer, and victory.

Do not finalize ornamental detail before the shortage-diagnosis and trade-route workflows test successfully.

No production raster may be stretched, upscaled, downscaled, or otherwise resampled. Generate dedicated native-size and aspect-ratio variants as required.

## 25. Reference mockups

The generated images are visual direction references, not production-ready layouts or final UI assets. Text, exact numbers, spacing, and interaction states must be recreated natively in UMG/Slate.

### Main city HUD

![Main city HUD reference](Images/UI/hansa-ui-main-city-hud.png)

### City market

![City market reference](Images/UI/hansa-ui-city-market-v2.png)

### European trade map

![European trade map reference](Images/UI/hansa-ui-trade-map.png)


### EMVP-P21 implementation anchor

The shared native component contract and selected ImageGen references are recorded
in [EnhancedMvpUiSystem.md](Development/EnhancedMvpUiSystem.md). The adopted project
fonts are Source Serif 4 Semibold (headings and tabular Data), Atkinson Hyperlegible
Regular (Body/Caption), and Noto Sans Symbols 2 (legacy symbol fallback). Existing
palette, spacing, accessibility and responsive rules above remain authoritative.


### Production-unit inspector anchor (2026-09-09)

The approved [bakery panel](Images/UI/Bakery/bakery--panel--baking--1024x1536--v1.png)
extends the existing inspector family to farms, mills, bakeries and generic recipe
units. Preserve its navy identity header, engraved good ports around a batch ring,
recipe band, stock/reservation ledger, workforce, production record and causal
card. Main actions are pause/resume, storage, production chain, pin and frame.
Secondary details are available through the header information control.

The ring represents batch progress, never productivity. Available input excludes
reservations; reserved-for-this-batch belongs to this unit, and output stock is
separate from lifetime batch output. Unknown inventory is explicitly unavailable.
Native geometry and shared typography/palette implement the reference within the
existing responsive host. At smaller sizes or large text, retain readable controls
with scrolling and focus reveal. Never resample the reference or source imagery.
See [implementation and verification](Development/ProductionInspectorImplementation.md).


### Compact production inspector revision (2026-09-10)

The user's latest production inspector direction supersedes the tall ledger
layout above. Default production cards are 320 by 480 Slate units at 1080p,
anchored bottom-right above the toolbar, bounded by available viewport height.
A restrained original Hansa worker engraving sits in a linen/brass arch above
the navy production-name header. The default body contains the input-good icons,
animated batch ring and output-good icons, with quantities and brief state text.
Pause/Details controls precede a compact coin/labor footer. Existing stock,
reservation, workforce breakdown, causal history and secondary actions expand
under Details. No second palette or font family is introduced.

Hovering the batch circle opens a navy/brass popup showing total batch duration
at 1x and current percent completed. Unknown operating cost remains a blank value
as explicitly requested; no construction cost, input-price estimate or invented
upkeep is substituted. Labor displays the actual required workforce and exposes
its allocated/required tier breakdown in a tooltip and Details. The initial
portrait is shared across bread-chain units. This request authorizes production
worker portrait decoration above the operational data.

See [compact panel component record](Development/CompactProductionInspector.md).


### Product construction tray revision (2026-09-10)

Bottom construction categories use icon-only native glyph buttons with localized
name tooltips and semantic/controller labels. Production opens compact end-good
selectors. Clicking Bread opens an above-row recipe panel: Farm, Mill, Bakery.
The same authored chain metadata drives Fish and Planks. Compact native building
tiles keep detailed costs, footprint, labor and availability in tooltips.
Select a building, move to preview its ghost, then press to construct. Holding
and moving stamps validated nonoverlapping buildings along the mouse stroke;
release stops the stroke and retains selection for another click. Escape cancels.
Road drawing retains its connected path preview and release-to-build behavior.
See [component inventory and references](Development/ProductConstructionTray.md).


### Residence inspector (2026-09-10)

Residences use a compact native inspector with an original citizen portrait above
the navy identity header, then current residents / maximum house capacity and
individual citizen needs. Each need has a good/service icon, text label, numeric
fulfillment percentage and bar. Tooltip details distinguish access, affordability
and reliability. Values come from the authoritative per-residence cohort, never
city averages. Unevaluated needs show a dash and explicit pending tooltip. Default
size is 320x520 Slate units, constrained by available height; scrolling and existing
accessibility preferences retain all needs/actions. Detailed causes and existing
upgrade/frame/pin actions remain under Details. See Development/ResidenceInspector.md.

### Construction header removal (2026-09-10)

The construction tray has no top instruction/Collapse bar. Building choices begin
immediately at the top of the expanded tray. Placement hints remain in building
tooltips; existing category toggles and Escape handle closing/cancellation.

### Camera drag control (2026-09-10)

Hold the right mouse button over the world and move the pointer to translate the
camera opposite to the pointer: dragging right moves left, dragging up moves backward.
Movement follows camera yaw and remains within city bounds. A stationary held
pointer does not pan; edge scrolling is suppressed during the gesture.
Release, leaving the world viewport, or losing application focus ends the drag.
Right press retains construction cancellation. Keyboard/controller panning remains
the non-drag alternative. Drag sensitivity is authored on the camera pawn.

### Selected market demand inspector (2026-09-10)

Selecting a completed market opens a compact native linen inspector with a navy
identity header, labeled product glyphs, and rounded supply-fulfillment bars.
Show the numeric percentage and supplied / required units beside each good.
The scope is explicitly city-wide, using a rolling window of 30 game days:
sum consumed / sum required per good across all recorded consumption ticks.
Show the actual recorded duration until a full window is available. Do not substitute stock, incoming
shipments, population satisfaction, or an unweighted mean of residence percentages.
Services and prospective needs in empty homes are excluded. Pending evaluation,
no demand, and unavailable city data are explicit; none means fully supplied.
Use existing typography/palette and focusable rows with a visible outline;
Details exposes causes, related market view, history and existing actions.
See [component record](Development/MarketInspector.md).


### Top menu revision (2026-09-10)

The left group shows icons followed by total player money, signed money change
across 30 game days, and total population in the current city. The center top row
shows Bread alone by default, centered as its existing bread image plus a signed
supply-minus-demand rate (units per game day). Empty product slots do not render
or occupy space; directly below are laborer and wealthy citizen resident counts. The
current two-tier scenario maps wealthy citizens to artisans, explained in the
tooltip. Top-right controls use individually ImageGen-generated pause, play,
double and triple triangle icons, with tooltips explaining pause, 1x, 4x and 12x; selection and focus remain
visible. City/date and existing navigation remain in a secondary row. Every metric
has an explanatory tooltip. Unknown values display a dash. See Development/TopMenu.md.


### Three separate top panels (2026-09-10)

The top menu uses three independent compact navy/brass panels anchored top-left,
top-center and top-right, with transparent world-visible gaps. No full-width
surface connects them. Preserve all existing content and tooltips: money, monthly
change, population and city controls left; products, citizen tiers and date/time
center; speed, connection and navigation right. Navigation wraps inside its panel
at compact widths and large text. Existing artwork replacement remains governed
by the mandatory ImageGen rule and the unresolved native-size constraint.

## GUI ImageGen resizing exception — approved 2026-09-10

For GUI icons and GUI images, the user authorizes proportional resizing of genuine ImageGen artwork when the generator cannot deliver the requested native display size. Request the closest practical size first. Preserve the original generated master and aspect ratio; transparent-margin cropping and high-quality proportional resampling into documented display-size variants are allowed. Review each result at actual display size, including small UI scale, on its intended background. Regenerate unreadable artwork rather than accepting blurred details. Never stretch, squash, or replace the generated artwork with manually drawn icons. Record generated dimensions, crop, output dimensions, prompt, and quality review. This exception supersedes conflicting no-resampling and native/vector icon advice in this document for GUI images only. Other game imagery retains its existing rules.

### Compact Road control (2026-09-10)

The Roads tray uses an icon-only button sized to its existing 48-unit icon.
Its localized name, costs and placement hints remain in the tooltip. The complete
secondary action panel (Grid, Road overlay, Favorite, Compare, Rotate, Repeat,
Build here and Cancel) is collapsed for Roads and excluded from controller focus.
Road path drawing, release-to-build, keyboard shortcuts and Escape remain available.
This is a native layout correction reusing the existing artwork and shared states;
no new raster asset or visual reference is introduced.

### Compact building controls for all categories (2026-09-10)

Apply the compact Road control revision to every building category, including
Residences, Production, Storage, Harbor and Civic/Market. All building choices
are 48x48 icon-only buttons using existing artwork, with names and details in
tooltips. No category displays the secondary action row. Existing hover, pressed,
selected, disabled, warning/error and focus outlines remain native shared states.

Ctrl + right-button drag rotates the camera around its current focus using
horizontal pointer movement (right increases yaw, left decreases it). Either Ctrl
key works; pressing/releasing Ctrl during a held drag switches rotation/panning.
Vertical movement changes pitch (2026-09-12 user clarification): dragging up tilts
the viewing direction upward; dragging down tilts it downward. Pitch starts at
-55 degrees and is clamped to -80 through -15 degrees to avoid flipping or looking
below the focus plane. Diagonal drags change yaw and pitch independently, preserving
the current focus and zoom distance. Rotation suppresses edge scrolling
and ends on the same release, viewport-leave and focus-loss conditions as panning.
Rotation sensitivity is authored through DragRotationDegreesPerPixel on the pawn.

### Construction building inspector (2026-09-11)

Under-construction buildings reuse the ordinary compact building inspector shell
and existing worker portrait. Its animated circle shows elapsed / total
construction ticks, with construction wording and a build-time tooltip. Hide
production-only ports, records, cost/labor footer and pause controls during
construction. Details retains cancellation refunds and the confirmed cancel
action. Completion returns to the building's ordinary operating inspector.
This is reuse of the approved component design and artwork; no new raster asset.

### Residence rolling consumption (2026-09-11)

Residence product rows now use that residence's actual consumed / required
quantities over the last 30 game days, with a percentage and quantity caption.
The state area shows recorded duration until a full window exists. Empty homes
show no demand or pending history, never a prospective product percentage.
Basic services retain their current metric with an explicit current-service
label; no product quantities are invented for services. Reuse the approved
portrait, icons, typography, bars, shell and interactions. The normal residence
host is 320x600 Slate units, bounded by available viewport height and scrollable;
quantity rows and controller focus remain reachable at compact resolutions.
See Development/ResidenceRollingFulfillment.md.


### Production product stock popups (2026-09-11)

Hovering or keyboard/controller focusing any input or output good reuses the
compact inspector's navy/brass tooltip with native Chalk Body text:
`{quantity} in storage` and `{quantity} in markets` on separate lines.
Storage counts the selected building's physical input/output buffers, including
reserved goods; a shared buffer is counted once. Markets counts the city's shared
market pools plus actual loaded cargo travelling to/from those pools, including
local deliveries and intercity vehicles. Pending pickups remain in source stock;
completed deliveries count at their destination. Unknown quantities say Unavailable.
The popup refreshes with the presenter while open. Existing artwork and native
surface/focus styles are reused; no raster or new visual family is introduced.
