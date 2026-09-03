# Hansa Content Root

All project-owned Unreal assets live below this directory. See `Docs/Development/RepositoryConventions.md` before adding or moving content.

The authoritative MVP catalogue is Primary Data Asset content under `Core/Goods`, `Core/Recipes`, `Core/Buildings`, `Core/Needs`, `Core/PopulationTiers`, and `Core/CityMarkets`. Its current reviewed inventory is 10 goods, 8 recipes, 14 buildings, 5 needs, 2 population tiers, and 4 city-market profiles (43 assets total). Runtime identity comes from each asset's stable ID, never its filename or package path.

All 14 building assets author deterministic resource costs, a Pfennig cost, build ticks, and a bounded cancellation-refund policy. The S06-P01 one-time field migration is documented in `Docs/Development/Construction.md`; it deliberately updates only those construction policy fields and never replaces an existing package wholesale.

The initial buildable-world presentation is the World Partition level `World/Cities/Lubeck/L_Lubeck_MVP`. Its fixed C++ foundation is representative placeholder topology for S05 and is not final environment art or authoritative simulation state.
