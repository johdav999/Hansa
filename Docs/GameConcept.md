# Hansa — Game Concept

## High concept

**Working title:** *Hansa: League of Merchants*

*Hansa* is an Anno-inspired city-builder and economic strategy game set during the rise and height of the Hanseatic trade network, approximately 1250–1500. Each player leads a merchant house that develops a home city and owns workshops, warehouses, ships, caravans, and political privileges across northern Europe.

The player does not rule an entire country. Power comes from making cities depend on the player's goods, controlling trade routes, shaping the Hanseatic League, and turning commercial success into urban and political influence.

The game supports solo play against AI, cooperative play, and competitive multiplayer. A standard campaign is a historical sandbox rather than a scripted reenactment: geography and industries are grounded in the period, while prices, alliances, crises, and the eventual dominant merchant houses emerge from the simulation.

## Anno-inspired design target

The game should be immediately understandable to players who enjoy Anno, without copying a specific title's setting, art, interface, text, or content. The familiar foundation is a readable city-builder driven by population needs and production chains. The improvements come from making trade, prices, foreign cities, and politics part of one coherent simulation.

### Familiar foundation

- Place roads, residences, public services, workshops, farms, warehouses, markets, and harbor buildings.
- Satisfy population needs to attract residents, upgrade houses, and unlock new buildings and goods.
- Balance workforce tiers, building upkeep, production ratios, storage, and transport carts.
- Construct multi-step chains in visually readable layouts.
- Set up automated trade routes with ships and wagons.
- Settle or develop new regions to obtain resources unavailable at home.
- Compete over space, resources, trade access, and economic milestones.
- Watch a dense, attractive medieval city grow from a small trading post.

### Improvements over the familiar formula

| Familiar limitation | Hansa's approach |
| --- | --- |
| Foreign cities often behave like fixed shops | Every important city produces, consumes, grows, and changes prices |
| Prices are frequently static or only event-driven | Stock, citizen needs, industry, incoming cargo, seasons, and policy determine local prices |
| Trade routes mainly move goods between player-owned settlements | Routes connect home cities, foreign branches, Kontors, inland markets, and other players |
| Land logistics are secondary | Roads, rivers, wagons, barges, transshipment, and sea routes form one network |
| Population needs are mostly upgrade checklists | Needs also create real demand, affordability pressure, migration, unrest, and business opportunities |
| Production statistics can hide the cause of a shortage | The UI explains missing inputs, transport delay, workforce, reserves, and price causes |
| Expansion usually means colonizing empty land | Expansion can mean building a district, buying property, winning privileges, founding a branch, or dominating a route |
| Defeat in multiplayer can become irreversible early | Credit, contracts, leasing, joint ventures, and restructuring create comeback paths |
| AI may feel economically disconnected | Major AI houses use the same goods, routes, money, privileges, and market rules |

The intended balance is roughly **60% readable city-building and production optimization, 25% trade and logistics, and 15% politics, research, and strategic disruption**. Economic depth should emerge from understandable systems, not from spreadsheet micromanagement.

## Design pillars

1. **Build beautiful, productive cities.** Workshops, housing, roads, harbors, markets, and public works form readable production and service layouts.
2. **Goods must physically move.** Profit depends on ships, wagons, warehouses, seasons, distance, risk, and route capacity.
3. **Every city has a living market.** Citizens, industries, governments, and rival merchants consume goods. Local stock and demand drive prices.
4. **Information is a resource.** Market reports become less reliable with distance and age. Fast couriers and trade offices create an advantage.
5. **Economic power becomes political power.** Privileges, guilds, councils, embargoes, and League decisions can be as important as production.
6. **There is no single correct victory path.** A player can dominate through wealth, logistics, industry, politics, or civic legacy.

## Player fantasy

The player begins with a small family business, a warehouse lease, and one modest vessel or wagon. Over time, the house can become:

- a Baltic bulk-goods empire;
- a producer of valuable cloth, metalwork, or ships;
- the indispensable carrier between eastern resources and western consumers;
- a political dynasty that controls city councils and League assemblies;
- a respected civic patron whose home city becomes the greatest city of the north.

The intended feeling is: **find an opportunity, construct the system that exploits it, then watch that system alter the world around it.**

## Campaign structure and time

- The campaign begins between 1250 and 1350, depending on scenario.
- Time runs in pausable real time with selectable speed.
- One in-game year contains four meaningful seasons.
- Harvests, fishing, storms, ice, consumption, and sailing conditions vary by season.
- A standard game should last roughly 12–20 hours; shorter scenarios and longer grand campaigns are available.
- The world continues without the player: cities grow or decline, competitors trade, shortages spread, and political relationships change.

## Core game loop

1. Read current or reported prices in nearby cities.
2. Identify a shortage, surplus, or production opportunity.
3. Buy goods or construct the required production chain.
4. Hire workers and secure raw materials, storage, and transport capacity.
5. Establish a manual or automated sea/land trade route.
6. Manage delays, spoilage, tolls, piracy, weather, and rival behavior.
7. Sell into local demand without flooding the market.
8. Reinvest profit into capacity, technology, privileges, and city development.
9. Use economic leverage to influence councils, guilds, and the Hanseatic assembly.
10. Adapt as prices, population, laws, and routes change.

## The map

The main map covers northern and central Europe, from Novgorod in the east to London and Bruges in the west, and from Bergen in the north to major inland German and Polish markets in the south. Peripheral markets can extend toward France, Iberia, and Italy in late-game scenarios.

The world is a network of cities, rural resource sites, navigable coastlines and rivers, roads, straits, and sea regions. Travel happens on visible routes, but cities use a detailed buildable district view.

### City categories

Historical status is represented so that a Hanseatic town, a foreign Kontor, and a valuable non-Hanseatic market do not play identically.

| Category | Examples | Gameplay role |
| --- | --- | --- |
| Leading Hanseatic cities | Lübeck, Hamburg, Bremen, Danzig/Gdańsk, Cologne | Major councils, large markets, League influence |
| Baltic member cities | Rostock, Wismar, Stralsund, Greifswald, Visby, Riga, Reval/Tallinn | Regional production and Baltic routes |
| Inland member cities | Lüneburg, Brunswick, Magdeburg, Dortmund, Münster | Salt, metals, cloth, grain, and land-route hubs |
| Kontor cities | London, Bruges, Bergen, Novgorod | Foreign privileges, restricted compounds, critical imports/exports |
| Partner and rival markets | Stockholm, Kraków, Antwerp, Amsterdam, Edinburgh, Bordeaux | Demand centers, alternate routes, and outside competition |

Names can display in their period-appropriate form while modern names appear in tooltips and accessibility/search options.

### Geographic systems

- **Sea regions:** Baltic Sea, Gulf of Finland, Gulf of Riga, Danish straits, North Sea, English Channel.
- **Rivers:** Elbe, Weser, Rhine, Vistula, Oder, Daugava, and selected connected waterways.
- **Land routes:** roads connect cities, mines, forests, farms, and river ports.
- **Seasonality:** winter ice restricts some eastern ports; storms and prevailing conditions change route risk and duration.
- **Chokepoints:** straits, bridges, river mouths, and toll stations can be negotiated, blockaded, or bypassed at a cost.

## City building

Each major city has a buildable area divided into plots or districts. In the player's home city, construction uses direct Anno-style placement and road connections. In foreign cities, the player begins with leased property around a warehouse or Kontor and must earn the right to expand. This preserves a satisfying city-building canvas while making commercial expansion feel different from founding another copy of the home city.

### Residence progression and needs

Residences fill when their needs are affordable and available within service range. A full residence supplies workforce, taxes, and local demand. Homes upgrade only when the player chooses, preventing an automatic upgrade from unexpectedly destroying the workforce balance.

| Tier | Role | Example basic needs | Example advancement needs |
| --- | --- | --- | --- |
| Laborers | Farms, docks, construction, basic workshops | Bread/grain, fish, firewood, simple clothing | Beer, market access, chapel |
| Artisans | Skilled workshops and guild production | Bread, varied food, beer, linen, shoes | Candles, tools, church, bathhouse |
| Merchants | Trade, administration, finance | Quality food, cloth, candles, wine | Glassware, books, guildhall, school |
| Patricians | Investment, council influence, advanced services | Fine cloth, wine, imported food, glass | Jewelry, spices, prestige services, civic beauty |

Needs have three effects at once:

1. **Access:** Is the product or service available to the residence?
2. **Affordability:** Can the household tier regularly pay its local price?
3. **Satisfaction:** Does supply remain reliable and is the neighborhood desirable?

This improves on a binary needs checklist. A city may have bread in its market, but high prices can still drive hunger, wage pressure, or emigration. The player can respond by increasing supply, subsidizing a staple, raising wages, or accepting slower growth.

### Roads, service range, and internal logistics

- Every active building requires a road, quay, or navigable waterfront connection.
- Production buildings send carts or porters to nearby warehouses and input stores.
- Congestion, travel time, loading capacity, and warehouse queues affect actual throughput.
- Markets distribute household goods within a service area; specialized halls extend capacity for dense districts.
- Paved roads, bridges, cranes, and larger warehouses improve flow without making distance irrelevant.
- A logistics overlay shows each trip, delay, queue, missing input, and suggested remedy.

### Islands, regions, and continental geography

The map is not limited to tropical-style islands. Large coastal regions, river valleys, peninsulas, and islands act as buildable territories. Each region has finite fertile land, resource deposits, harbor sites, and political ownership. This retains the enjoyable Anno decision of settling elsewhere for missing resources while fitting northern European geography.

### Districts and buildings

- **Harbor:** quays, cranes, shipyards, fisheries, customs offices, lighthouse.
- **Market:** market hall, merchant office, scales, counting house, exchange.
- **Warehouse:** dry storage, granary, cellar, bonded warehouse, cold/salted storage.
- **Production:** mills, bakeries, breweries, smithies, tanneries, weaving halls, ropewalks, sawmills.
- **Residential:** worker housing, artisan homes, merchant houses, patrician estates.
- **Civic:** church, guildhall, school, hospital, fire watch, walls, public well.

Buildings affect nearby plots. A tannery is cheap near water but reduces residential desirability; a market raises land value and foot traffic; dense wooden neighborhoods increase fire risk; docks need direct water access.

### Population groups

| Group | Economic role | Main demands |
| --- | --- | --- |
| Laborers | Basic workforce, docks, construction | Grain, bread, fish, beer, firewood, simple cloth |
| Artisans | Skilled production and guilds | Better food, tools, leather goods, cloth, candles |
| Merchants | Administration, finance, trade offices | Wine, quality cloth, wax, glassware, imported food |
| Patricians | Council power and high investment | Luxury cloth, spices, jewelry, wine, prestige goods |
| Clergy and institutions | Education, welfare, legitimacy | Grain, fish, wine, wax, books/parchment |

Households consume goods, supply labor, pay rents and taxes, and react to prices. Sustained affordability and employment attract migration. Hunger, disease, fire, unemployment, and excessive taxation cause unrest or emigration.

Population demand should be forecastable like a city-building production ratio. The interface shows consumption per minute, current supply, reserve days, price trend, and the number of additional residences a chain can support. Dynamic behavior adds depth after the player understands the baseline; it does not obscure it.

## Goods and production chains

Production is recipe-based. Each building consumes inputs, labor, time, fuel, and sometimes tools. Regional productivity creates natural trade: not every city should efficiently make everything.

Each building has a clear cycle time and nominal throughput. The construction menu can display simple ratios such as “one mill supports two bakeries,” while actual output reflects workforce, transport, maintenance, input quality, and local conditions. Players can optimize layouts without needing an external calculator, then use deeper overlays when a chain underperforms.

### Essential chains

| Chain | Inputs | Processing | Outputs and uses |
| --- | --- | --- | --- |
| Bread | Grain + fuel | Mill → bakery | Flour → bread; staple citizen demand |
| Beer | Barley/grain + water + fuel | Brewery | Beer; broad urban demand |
| Preserved fish | Fish + salt + barrels | Salting house | Salted fish; staple food and long-distance trade |
| Timber | Logs | Sawmill | Planks; construction, barrels, ships |
| Barrels | Planks + iron fittings | Cooper | Barrels; required to transport many goods efficiently |
| Shipbuilding | Planks + iron + rope + sailcloth + pitch | Shipyard | Cogs, river craft, later specialized vessels |
| Linen | Flax | Retting/spinning → loom | Linen cloth; clothing and trade |
| Rope and sails | Hemp/flax | Ropewalk/loom | Rope and sailcloth; ship upkeep and construction |
| Wool cloth | Wool | Fulling/spinning → loom | Cloth; large western and urban demand |
| Dyed cloth | Cloth + dye + alum | Dyer | High-value finished cloth |
| Leather goods | Hides + tannin | Tannery → workshop | Leather, shoes, harnesses |
| Iron tools | Iron ore + charcoal | Bloomery/smelter → smithy | Iron, tools, fittings, anchors |
| Fuel | Timber | Charcoal burner | Charcoal; metalworking and some workshops |
| Wax goods | Beeswax | Chandler | Candles; homes, churches, workshops |
| Glassware | Sand + potash + fuel | Glassworks | Glass; urban and luxury demand |
| Amber goods | Amber | Jeweler | Jewelry and prestige exports |
| Fur garments | Furs + cloth/leather | Furrier | Warm clothing and luxury goods |
| Wine trade | Grapes/wine from southern markets + barrels | Cellar/merchant | Urban, church, and luxury demand |

### Regional advantages

- Lüneburg and other salt sources support fish preservation.
- Baltic and eastern regions supply grain, timber, wax, furs, flax, hemp, amber, and pitch.
- Scandinavia supplies fish, timber, iron, and selected metals.
- England and Flanders are important to wool and cloth trade.
- The Rhineland and western/southern trade links supply wine and crafted goods.
- Large cities create strong demand for food, fuel, construction material, and finished products.

Advantages are strong but not absolute. Research, investment, labor skill, and access to fuel can make an unusual industry viable at a higher cost.

### Quality and spoilage

Goods can have quality tiers when that creates meaningful decisions. Food decays; cloth can be damaged by moisture; poor barrels increase losses. Quality depends on worker skill, input quality, building condition, and production method. Better warehouses and packaging reduce deterioration.

## Market simulation

Every city maintains its own stock, recent trade volume, expected supply, and demand for every relevant good. There is no universal price.

### Sources of demand

- household consumption by population group;
- industrial input orders;
- construction and shipbuilding;
- city institutions, garrisons, churches, and festivals;
- AI merchant purchases and exports;
- emergency stockpiles during war, winter, or shortages.

### Sources of supply

- local farms, fisheries, mines, forests, and workshops;
- player and AI deliveries;
- background regional trade representing small merchants;
- city reserves released during severe shortages.

### Price model

For each city and good, the simulation tracks:

- current stock;
- desired reserve stock;
- expected production and incoming shipments;
- current consumption and industrial demand;
- recent average price and volume;
- taxes, tariffs, privileges, and embargoes.

A conceptual price calculation is:

`Local price = base value × scarcity × demand pressure × season × local modifier × policy modifier`

Scarcity compares available stock with the city's desired reserve. Demand pressure considers recent unmet orders and expected consumption. Price movement is smoothed and capped per simulation step so a small delivery cannot produce wild oscillation. Each good has its own elasticity: grain reacts sharply during famine, while luxury demand falls when prices become excessive.

### Market behavior

- Selling a large cargo lowers the local price as the market absorbs it.
- Buying heavily raises the price and can deprive local industries or citizens.
- A shortage of one input raises the price of its downstream products after a delay.
- Population growth increases staple demand and expands the market for advanced goods.
- High prices attract AI imports; low prices encourage exports and discourage production.
- Traders can place buy/sell thresholds and quantity limits rather than only trading manually.
- Market halls show actual local prices; distant prices arrive as dated reports and may be wrong by the time a ship arrives.
- Essential goods can be reserved by minimum stock so an automated route does not accidentally export a city's entire food supply.
- The player can choose local distribution, export priority, or industrial priority per good.

### Events and fluctuations

Harvest failure, exceptional catch, mine collapse, city fire, plague, war, piracy, frozen ports, new tolls, guild disputes, and festivals temporarily change supply or demand. Events alter the underlying simulation instead of applying unexplained random price percentages.

## Trade and logistics

### Sea trade

- Ships have cargo capacity, draft, speed, crew need, durability, upkeep, and weather tolerance.
- Early ships favor the cog and smaller coastal/river craft; later research unlocks larger or more specialized designs.
- Deep-draft ships cannot reach every river port.
- Crews need wages and provisions. Damage and fouling reduce performance.
- Routes can specify stops, cargo rules, minimum reserves, buy/sell thresholds, and seasonal behavior.
- Route setup offers both a simple Anno-style mode (load, unload, minimum reserve) and an advanced mode (price limits, conditional stops, substitutions, season rules).
- Convoys improve protection but may be slower and easier for competitors to observe.

### Land and river trade

- Wagons are flexible but expensive per unit of bulk cargo.
- Pack trains work on poor roads but carry less.
- River barges efficiently connect inland markets to seaports where geography permits.
- Roads, bridges, tolls, mud, snow, and security affect travel time and loss risk.
- Transshipment between ship, barge, and wagon costs time, labor, and warehouse space.

### Risk and security

Pirates, privateers, bandits, storms, fire, and seizure threaten cargo. The player can insure shipments, hire guards, join a convoy, improve intelligence, negotiate safe-conduct, or accept the risk. Combat exists to protect commerce, not as the main game.

## Politics, privileges, and the League

The Hanseatic League is a changing association of cities rather than a unified country.

- City councils grant warehouse access, tax exemptions, market stalls, land, and monopolies.
- Guild relationships affect labor, production licenses, and political support.
- Foreign rulers grant or revoke Kontor privileges.
- The Hanseatic assembly can vote on embargoes, collective convoys, war funding, standards, and admission or expulsion.
- Players spend influence, call in favors, build coalitions, or use economic leverage.
- Aggressive market manipulation can bring profit but damage reputation and provoke regulation.

Reputation is tracked separately with cities, social groups, the League, and foreign rulers. A player may be admired at home and distrusted abroad.

## Research and progression

Research represents accumulated craft knowledge, institutions, contacts, and business practice—not modern laboratory science. Knowledge comes from guilds, schools, experienced crews, foreign offices, master artisans, exploration, and targeted investment.

### Research branches

| Branch | Example advances |
| --- | --- |
| Commerce | Double-entry bookkeeping, standardized weights, bills of exchange, marine insurance, improved market reports |
| Maritime | Better rigging, hull construction, navigation tables, convoy doctrine, dry docks |
| Production | Water-powered mills, improved looms, blast-furnace methods, better tanning, standardized barrels |
| Logistics | Warehouse cranes, inventory ledgers, paved yards, cold cellars, scheduled routes |
| Civic | Fire watch, sanitation, hospitals, schools, reinforced quays, planned districts |
| Diplomacy | Foreign agents, legal expertise, safe-conduct treaties, guild negotiation, League procedure |

The tree contains meaningful choices and cross-branch requirements. Technology should improve efficiency or unlock a new strategy, not merely add repeated percentage bonuses. Some knowledge spreads between cities over time, while trade secrets can give a temporary lead.

## Competition and AI

AI players use the same markets, routes, production rules, money, and political systems as human players. They do not receive invisible goods.

### AI merchant archetypes

- **Bulk trader:** grain, timber, salt, fish, and high-capacity routes.
- **Industrialist:** vertically integrated workshops and urban property.
- **Carrier:** route capacity, contracts, and logistics services.
- **Financier:** credit, insurance, market speculation, and political influence.
- **Opportunist:** reacts rapidly to crises and distant price differences.
- **Dynast:** prioritizes councils, reputation, and League leadership.

Difficulty changes planning quality, risk tolerance, information delay, and coordination—not basic economic rules. AI houses can cooperate, form temporary cartels, undercut one another, or target the leader politically.

## Multiplayer

- 2–8 players, with remaining slots optionally filled by AI.
- Competitive, cooperative, or team play.
- Simultaneous pausable real time; multiplayer pause and speed rules are configured by the host.
- Public contracts let players request delivery of goods or transport capacity.
- Joint ventures allow shared investment in convoys, Kontors, or public works.
- Trade agreements, loans, embargoes, and political promises are supported by game systems.
- Defeated or indebted players can recover through contracts, employment, mergers, or a new branch rather than being immediately removed.
- Optional anti-grief rules protect starting markets and limit destructive dumping in casual games.

For synchronization, the economy should run on a deterministic fixed simulation step under server authority. Clients submit orders and receive economic state changes; prices must never depend on frame rate.

## Victory conditions

A campaign can enable one or several victory paths. Victory requires both a major objective and a minimum house stability/reputation threshold, preventing a player from winning through a brief exploit and immediate collapse.

### Merchant prince

Reach a target net worth, annual profit, and diversified income while remaining solvent for several years.

### Trade hegemon

Control a required share of trade volume across several regions and maintain active offices in all major Kontors.

### Industrial power

Become the leading producer in several advanced goods and complete multiple integrated production chains at high quality.

### First among equals

Gain dominant League influence, lead successful assemblies, and secure a lasting network of city privileges.

### City of prosperity

Transform the home city into the most populous, healthy, educated, and prosperous northern city through public works and affordable supply.

### Grand undertaking

Complete a scenario-specific legacy project, such as a great harbor, cathedral funding, fortified Kontor, or continent-spanning protected trade network.

### Scenario victories

Survive a blockade, restore a declining city, break a monopoly, feed multiple cities through a famine, or preserve a trade network during war or plague.

If time expires, a transparent legacy score combines wealth, trade, production, civic development, political influence, reputation, and resilience.

## Failure and recovery

Bankruptcy should be dangerous but not always final. Players can sell property, refinance, take contracts, lease ships, accept a patron, or restructure debt. Final defeat occurs when the house has no assets, no credit, and no viable contract for a sustained period.

This creates stories of recovery and makes multiplayer less likely to eliminate someone early.

## User interface priorities

- A route-planning map that previews time, capacity, tolls, risk, and expected—not guaranteed—profit.
- City market screens showing price history, stock, consumption, production, known incoming cargo, and report age.
- Production-chain overlays that reveal bottlenecks and downstream effects.
- Alerts explain causes: “bread price rising because grain reserve is below 12 days,” not merely “price +20%.”
- Automation rules remain inspectable and can be overridden per route or building.
- A timeline records contracts, major price shocks, political decisions, and shipment losses.
- Construction and production menus expose building cycle times, workforce, transport demand, and supported population in a consistent format.
- A needs panel connects each population tier directly to the chains and trade routes supplying it.
- Beauty and readability matter: busy streets, carts, harbor activity, building upgrades, and visible cargo should make economic success legible in the world.

## Recommended first playable scope

The complete vision is large. Development should begin with a vertical slice that proves the economy and physical trade before expanding the map.

The authoritative boundary for this first playable—including its editor and MCP/semantic/screenshot test workstreams—is defined in [MVP.md](MVP.md). The summary below describes its game-design direction; where scope differs, `MVP.md` controls the MVP implementation.

### Vertical slice

- 4 cities: Lübeck, Hamburg, Lüneburg, and Rostock.
- Connected sea, river, and land routes.
- 10 goods: grain, flour, bread, fish, salt, timber, planks, iron, tools, and beer.
- 8 production buildings plus housing, market, warehouse, dock, and merchant office.
- Laborers, artisans, and merchants as population groups.
- Anno-style residence needs, manual upgrades, workforce balance, roads, carts, service range, and building placement.
- Local stock, consumption, production, price history, and AI background trade.
- One player ship, one wagon route, and route automation.
- One AI rival using the same economy.
- Seasonal harvest, winter demand, storms, and one shortage event.
- Save/load and deterministic simulation tests.

### Early access foundation

- 12–16 cities across the Baltic and North Sea.
- 25–35 goods and the main historical chains.
- Four AI merchant archetypes.
- City privileges, reputation, contracts, and a first version of League politics.
- 2–4 player multiplayer.
- Three victory conditions and several scenarios.

### Full campaign target

- 30–50 significant cities plus smaller resource settlements.
- Complete Kontor and inland trade systems.
- 50–70 goods, quality, spoilage, finance, insurance, and advanced politics.
- 2–8 player multiplayer, teams, cooperative campaigns, and robust AI.
- Multiple start dates, scenario goals, and victory paths.

## Technical design principles for Unreal Engine

- Store goods, recipes, buildings, cities, ships, technologies, and events as data-driven definitions rather than hard-coded classes.
- Keep the economic simulation separate from actors, rendering, and UI.
- Advance the simulation on a fixed tick and use deterministic numeric rules suitable for multiplayer and replays.
- Aggregate citizens into households or cohorts; do not simulate every citizen as an economic agent.
- Aggregate background merchants while keeping player and major AI shipments physical and visible.
- Record causal data for price and production changes so the UI and AI can explain decisions.
- Treat save files as versioned simulation snapshots with migration support.
- Build automated tests around conservation of goods, route delivery, price stability, bankruptcy, and multiplayer determinism.
- Keep nominal production ratios simple and authored; apply simulation modifiers after the baseline so tooltips remain trustworthy.

## Design guardrails

- Do not let players profit from teleporting goods between warehouses.
- Do not make every shortage a random event; most should arise from production, logistics, consumption, or policy.
- Do not give every city identical production potential or consumption.
- Do not require constant manual loading once a route is configured.
- Do not turn dynamic pricing into stock-market micromanagement; building a functioning city remains the primary activity.
- Do not hide basic production ratios behind simulation complexity.
- Do not make war more profitable or mechanically deeper than trade.
- Do not pursue historical detail that cannot create a meaningful player decision.
- Keep the simulation understandable: players should be able to discover why a price changed.

## Unique selling proposition

Most city-builders focus on one settlement, while many trading games treat cities as static price lists. *Hansa* connects the two: citizens create demand, production reshapes cities, cargo physically changes regional markets, and commercial success changes political power. The player's trade network is not layered on top of the city simulation—it is what makes the cities grow.
