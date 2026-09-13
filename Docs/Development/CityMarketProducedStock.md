# City market produced stock — 2026-09-10

The city overview and full market read FHansaCityMarketProjection.CurrentStock.
Previously, FHansaMarketExecutor refreshed this value only on price-update ticks,
although production and consumption changed the inventory ledger every tick.
A completed bakery batch could therefore be absent from the current market view
until the next price update. The city overview also showed no production total.

Stock now refreshes from the same authoritative available inventory every tick.
Price history, price changes, background trading and remote report publication
retain their authored cadence. No inventory is credited twice and consumed bread
is not recreated. Existing saves need no schema migration.

The existing native city-market field grid now includes Produced so far (local
buildings), calculated from the same completed cycles and recipe output quantities
as the production inspector. Stock accounting explains why remaining available
stock can be lower: consumption, exports and reservations. Remote-city rows retain
their knowledge-filtered fields and do not disclose these local production totals.
This is a data/interaction correction using existing widgets; no raster assets,
palette, typography or generated reference changes are required.

A screenshot alone cannot establish whether the user's particular three breads
were already consumed. The normal simulation runs needs consumption after
production, so produced-so-far and remaining stock are deliberately separate.

Verification:
- Development Editor build succeeded.
- Hansa.Simulation.Market: 8 tests passed, including LiveProductionStock:
  three bakery batches appear as 3.0 bread before the first price update;
  price/history cadence stays unchanged; citizen consumption updates stock.
- Hansa.UI.CityOverview: 6 tests passed, including totals matched to local
  completed production, native widget semantics and remote information boundaries.- Hansa.UI.MarketTable: 5 tests passed, including production/consumption consistency.
- Native 1280x720 market capture inspected: new fields fit the existing field grid.
  Saved/P24/city-1280x720-market.png (2026-09-10).
  The broader RealViewport test fails later at retry recovery and population
  supply-chain navigation; this is not reported as an overall passing capture test.
- Native 1920x1080 market capture inspected: production and stock-accounting fields
  are readable without clipping. Saved/P24/city-1920x1080-market.png (2026-09-10).
  These captures verify layout in a paused initial session, not the three-bread
  scenario; the latter is proven by LiveProductionStock. The broader capture
  flow has the same later retry/supply-navigation failures at both resolutions.
