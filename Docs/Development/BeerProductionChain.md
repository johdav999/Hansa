# Expanded beer production chain

Status: implemented in runtime fixtures, authored seed/migration, construction and market presentation, automated tests, and GUI icon assets on 2026-09-12. The Hop Farm, Malt House, and Cooperage intentionally retain the standard fallback presentation until the 3D models below are delivered and approved; the Brewery and Lumber Camp already have promoted presentations.

## Production graph

Generic `Good.Grain` is deliberately shared: the same grain farm output can be sent either to the Mill for flour or to the Malt House for malt. `Good.Timber` is also shared: the Lumber Camp supplies both the Sawmill and the Cooperage.

| Phase | Building | Inputs per cycle | Outputs per cycle | Cycle | Workforce |
| --- | --- | --- | --- | --- | --- |
| Grain growing | Grain Farm | none | 6 grain | 60 ticks after starter balancing | 1 laborer |
| Hop growing | Hop Farm | none | 4 hops | 90 ticks | 4 laborers |
| Timber cutting | Lumber Camp | none | 6 timber | 100 ticks | 8 laborers |
| Malting | Malt House | 4 grain | 3 malt | 60 ticks | 3 laborers, 1 artisan |
| Cooperage | Cooperage | 3 timber | 1 empty barrel | 75 ticks | 3 laborers, 2 artisans |
| Brewing | Brewery | 3 malt, 1 hops, 1 empty barrel | 5 beer | 100 ticks | 4 laborers, 2 artisans |

The Beer construction selector contains the four dedicated stages `Hop Farm → Malt House → Cooperage → Brewery`. Grain Farm and Lumber Camp remain in the Bread and Planks selectors because they are shared upstream producers rather than duplicate buildings. The authoritative recipe graph still exposes both dependencies to the Malt House and Cooperage.

New stable IDs:

- goods: `Good.Hops`, `Good.Malt`, `Good.Barrels`;
- recipes: `Recipe.GrowHops`, `Recipe.MaltGrain`, `Recipe.MakeBarrels`;
- buildings: `Building.HopFarm`, `Building.MaltHouse`, `Building.Cooperage`;
- revised: `Recipe.BrewBeer` revision 2 and `Building.Brewery` revision 4 (including its separately reviewed production presentation).

## Approved Brewery presentation

The approved Brewery model is bound through `DA_Building_Brewery.PresentationMesh` to `/Game/Mesh/hansa-brewery-huexstrasse128/Production/SM_HansaBrewery_Production.SM_HansaBrewery_Production`. It uses a 4×4 parcel at native scale (12.24 × 15.55 × 15.58 m). Catalog v14 hash is `73EC37D013D49BA0`; the existing compatibility contract requires a new game. Brewing quantities, costs, workforce, and duration are unchanged by model promotion. See [promotion evidence](../../SourceArt/Buildings/hansa-brewery-huexstrasse128/PRODUCTION_PROMOTION.md).

## GUI component inventory and state ownership

- Screen shell: existing build tray, market ledger, and production inspector.
- Navigation: existing production-chain selector; Beer reports four dedicated stages.
- Panels and lists: existing native construction cards and market rows.
- Controls: existing native card, tab, search, sort, and placement controls.
- Status and feedback: existing default, hover, pressed, selected, disabled, keyboard/controller focus, loading, warning, and error treatments. No state is baked into raster art.
- Reusable good icons: Hops, Malt, Barrels.
- Reusable building icons: Lumber Camp, Hop Farm, Malt House, Cooperage, Brewery.
- Decorative imagery: none added.

All eight selected masters are built-in ImageGen RGBA assets at 1254×1254. They live under `SourceArt/UI/Icons/` with sibling `.prompt.md` records. The proportional premultiplied-alpha pipeline produces 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, and 160 px square variants under `Content/Hansa/UI/Icons/`. The generated masters are production source art; the size variants are production-ready PNGs loaded by native Slate brushes.

Visual inspection passed for subject identity, centered silhouette, approved warm natural-material palette, genuine alpha, unclipped edges, and light/dark background use. The 32–48 px variants are the preferred card and ledger sizes. The 16–24 px building variants remain supplementary because only the broader building silhouette survives at that scale; native labels provide the required redundancy. Review sheet: `Docs/Images/UI/BeerChain/icons-actual-size.png`.

## 3D models still required

### Required building presentations

1. `SM_HansaHopFarm` and/or `BP_HansaHopFarm_Presentation`: a small worker hut, planted hop rows, poles, and trellis wires. The 4×4 footprint, entrance, road-facing edge, and harvest area must read at gameplay camera distance.
2. `SM_HansaMaltHouse` and/or `BP_HansaMaltHouse_Presentation`: brick-and-timber malt house with steep roof, kiln vent/cowl, grain intake, and malt dispatch. Fit the 3×3 footprint.
3. `SM_HansaCooperage` and/or `BP_HansaCooperage_Presentation`: open-front cooper's workshop with visible workbench, stave racks, hoops, and a barrel-in-progress. Fit the 3×3 footprint.
The Brewery now uses the promoted `SM_HansaBrewery_Production` mesh, and the existing Lumber Camp presentation is reused for timber production. Neither needs a duplicate model for this chain.

### Required reusable production/cargo props

- modular hop trellis: straight, end, and corner pieces plus mature hop-vine cluster;
- hop basket or bale for input/output visualization;
- generic grain sack reused at the Malt House, plus a distinct malt sack or malt pile;
- soaking vat, germination-floor dressing, kiln basket, and malt shovel;
- barrel kit: empty barrel, stacked empty barrels, loose staves, iron hoops, and barrel-in-progress;
- brewery kit: mash tun, copper kettle, fermentation vat, cooling/working tub, and full beer cask;
- timber/log stack reused from the Lumber Camp for the Cooperage input area.

Small dressing pieces should share materials and be instance-friendly. Dynamic quantities, status, and labels remain native UI/data and must not be baked into meshes or textures.

## Verification targets

- compile the 81-definition catalog in forward and reverse discovery order with identical registry hashes;
- run the 1,000-tick production test and observe completed Malt House and Cooperage cycles plus positive Hops and Beer stock (malt and empty barrels may be consumed immediately by the Brewery);
- verify all four Beer construction stages appear in order and their semantic actions are device-neutral;
- verify all thirteen market rows and all generated icon density files exist;
- after 3D delivery, test placement ghosts, footprints, collision, road access, input/output props, LOD/Nanite, and real-viewport readability at 1280×720 and 1920×1080.
