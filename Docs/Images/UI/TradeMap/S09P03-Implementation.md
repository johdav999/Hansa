# S09-P03 implementation

The production HUD now opens a native European trade-map shell from `HUD.TopStatus.TradeMap`. The existing market `Begin route` action opens the same screen while preserving the selected good as route-planning context.

`UHansaTradeMapPresentationModel` consumes immutable simulation projections and the compiled economic registry. It formats four city report states, two route modes, ordered stops, cargo actions, capacity, deterministic round-trip ticks, upkeep, reserve risk, and expected gross-profit ranges. Unknown reports never become zero; stale/estimated data receives explicit uncertainty wording and a widened planning treatment.

`SHansaTradeMap` draws city rings, city labels, sea/land connections, and route emphasis as native Slate geometry. The Simple editor exposes semantic controller actions for route/stop selection, load/unload cycling, quantity/reserve stepping, save, and start/pause. Save and start/pause submit `FHansaEditRouteCommand` and `FHansaSetRouteActiveCommand` through `UHansaRuntimeSimulationHost`; widgets never mutate simulation state directly.

The sole visual reference remains `Docs/Images/UI/hansa-ui-trade-map.png` at its existing native dimensions. It is reference-only, not imported or sampled by the runtime. No production raster, resize, stretch, or baked dynamic text was added.
