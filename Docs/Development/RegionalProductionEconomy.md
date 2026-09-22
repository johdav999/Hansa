# Regional production economy

Implemented from `RegionalProductionEconomy-Prompt1.md`. This is a data and simulation feature only; it adds no GUI artwork, workshop models, or map routes.

## Model

`ProductionChain.*` definitions describe ordered recipe stages. `Region.*` definitions describe a portfolio shared by six historically/geographically related cities, raw-resource endowments, source caps, and exchange capacity/delay. Each `City.*` market profile selects only a subset of region stages and may mark at most two signature families. Products are derived from recipe outputs; cities do not maintain a second produced-goods list.

Remote industries run atomic recipe cycles against the city's physical inventory. Inputs are consumed before outputs are deposited. Source recipes are capped by the region's reviewed endowment; `Absent` and `ImportOnly` never create supply. State records completed cycles, last output, and blockers. The legacy background-production fields remain serializable, but every regional city has them set to zero and the runtime suppresses legacy production for any city with industry bindings.

Regional exchange is not a shared inventory. Deterministic region/good/destination/source ordering matches surplus above source reserve to industry input demand first and market reserve second. Goods leave the source immediately, become an explicit shipment, and arrive no earlier than the next authored market update. Region capacity and transport loss are applied. Lübeck is a member for catalog/query purposes but is excluded from automated exchange and remote-industry simulation.

## Initial chains

| Chain | Ordered stages |
|---|---|
| Bread | GrowGrain → MillFlour → BakeBread |
| Fresh fish | CatchFish |
| Preserved fish | CatchFish → SaltedCatch |
| Planks | FellTimber → SawPlanks |
| Beer | GrowGrain, GrowHops → MaltGrain, MakeBarrels → BrewBeer |
| Firewood | FellTimber → SplitFirewood |
| Charcoal | FellTimber → BurnCharcoal |
| Tools | SourceIron, BurnCharcoal → SmithTools |
| Shoes | SourceRawHides, StripTanningBark → TanLeather → MakeShoes |
| Salt | ExtractSalt |

The four new source recipes are remote-only and have no player buildings. Import-only endowments never generate local raw supply.

## City-stage matrix

An asterisk marks a signature family. Entries name enabled stages, not a claim that the full chain exists inside one city.

| Region | City | Enabled production families and stages |
|---|---|---|
| Wendish/Lower Elbe | Lübeck | Bread*: MillFlour, BakeBread; Beer*: MaltGrain, MakeBarrels, BrewBeer; PreservedFish: SaltedCatch; Tools: SmithTools; Shoes: TanLeather, MakeShoes |
| | Hamburg | FreshFish*: CatchFish; Bread*: MillFlour, BakeBread; Beer: BrewBeer; Tools: SmithTools; Shoes: MakeShoes |
| | Lüneburg | Salt*: ExtractSalt; Bread: GrowGrain; Beer: GrowGrain, MaltGrain, BrewBeer |
| | Rostock | Bread*: GrowGrain; FreshFish*: CatchFish; Planks: FellTimber, SawPlanks; Shoes: SourceRawHides, StripTanningBark, TanLeather |
| | Wismar | FreshFish*: CatchFish; Beer: GrowHops, BrewBeer; Planks: FellTimber; Shoes: StripTanningBark, TanLeather |
| | Stralsund | PreservedFish*: CatchFish, SaltedCatch; Bread*: GrowGrain; Shoes: StripTanningBark, TanLeather |
| Prussian/Pomeranian | Danzig | Bread*: full; Beer*: MakeBarrels, BrewBeer; PreservedFish: SaltedCatch; Tools: SmithTools |
| | Elbing | Bread*: GrowGrain, MillFlour; Planks*: full; Beer: MakeBarrels |
| | Königsberg | Bread*: GrowGrain; Firewood*: full; Shoes: hides, bark, tanning |
| | Thorn | Bread*: GrowGrain, BakeBread; Beer: GrowHops, MaltGrain; Shoes: MakeShoes |
| | Stettin | Planks*: full; Charcoal*: BurnCharcoal; Tools: SmithTools; FreshFish: CatchFish |
| | Greifswald | FreshFish*: CatchFish; PreservedFish: SaltedCatch; Shoes: hides, MakeShoes |
| Livonian/Rus | Riga | Planks*: full; Bread*: GrowGrain, MillFlour; Shoes: tanning, MakeShoes; Tools: SmithTools |
| | Reval | FreshFish*: CatchFish; PreservedFish*: SaltedCatch; Planks: SawPlanks |
| | Dorpat | Bread*: GrowGrain; Firewood*: full; Shoes: hides |
| | Narva | Planks*: full; Charcoal*: BurnCharcoal; Shoes: bark |
| | Pskov | Bread*: GrowGrain; Shoes*: hides, tanning; Firewood: SplitFirewood |
| | Novgorod | Planks*: FellTimber; Shoes*: hides, bark; Tools: SourceIron, SmithTools |
| Scandinavian | Bergen | PreservedFish*: full; Planks*: FellTimber; Shoes: hides |
| | Oslo | Planks*: full; Firewood*: SplitFirewood; Shoes: hides, tanning |
| | Stockholm | Tools*: SourceIron, BurnCharcoal, SmithTools; FreshFish*: CatchFish; Planks: SawPlanks |
| | Visby | FreshFish*: CatchFish; PreservedFish*: SaltedCatch; Beer: BrewBeer |
| | Kalmar | Bread*: GrowGrain, MillFlour; Beer*: GrowHops, BrewBeer; Planks: FellTimber |
| | Malmö | Bread*: GrowGrain, BakeBread; Beer*: GrowHops, MaltGrain, BrewBeer; Shoes: MakeShoes |
| Western North Sea | Bruges | Bread*: MillFlour, BakeBread; Beer*: BrewBeer; Shoes: tanning, MakeShoes; Tools: SmithTools |
| | Antwerp | Beer*: MaltGrain, MakeBarrels, BrewBeer; Tools*: SmithTools; Bread: BakeBread |
| | London | Bread*: MillFlour, BakeBread; Beer*: BrewBeer; Tools: SmithTools; Shoes: MakeShoes |
| | Boston | Bread*: GrowGrain, MillFlour; FreshFish*: CatchFish; Beer: GrowHops |
| | King's Lynn | Bread*: GrowGrain; PreservedFish*: full; Beer: MakeBarrels |
| | Kampen | FreshFish*: CatchFish; Beer*: BrewBeer; Shoes: tanning, MakeShoes |

## Persistence and verification

Remote industries and shipments are authoritative, saved, semantically validated, projected, and included in the market hash. Save format 11 and fingerprint 24 require a new game. Accepted catalog v29 contains 163 definitions and pins registry hash `0698066FA49B59A2`; v28 remains the immediate previous incompatible catalog. `Hansa.Integration.Authoring.RegionalProduction.Catalog` covers counts, membership, subsets, signatures, Rostock's leather inputs, no legacy production faucets, compiler validity, and order-independent hashes. The full editor target links, the offline authoring commandlet saved 145 generated baseline packages, and the disk-reloaded catalog report verified reverse discovery order.
