# S08-P01 implementation record

## Runtime architecture

`UHansaCityOverviewPresentationModel` consumes the immutable authoritative simulation projection and economic registry. It publishes only UI-ready immutable snapshots and only when state changes. Population needs, production blockers, market stock/demand/incoming/price data, and active alerts are copied from the shared projection; Slate does not query mutable state or tick.

`SHansaCityOverview` uses one `SListView` for Population, Production, and Market, so row creation is virtualized. Changing tabs swaps the immutable item source and requests one list refresh. Off-screen rows can receive controller focus before Slate materializes them: the list selects and scrolls the stable row into view while preserving the semantic focus target. The root HUD exposes `HUD.TopStatus.CityOverview`, hosts the screen as a modal overlay, restores focus on close, and forwards production-building causal navigation to the existing inspector/world projection path. Population-need and market-supply links switch to and select the matching good's Production node; they are never misrouted through a residence ID.

Administration is a disabled `Future` placeholder and is excluded from activation and controller focus order.

## Native component contract

Six stable header summaries are always present. Population rows expose residents/capacity, workforce, migration, satisfaction, and separate need access/affordability/reliability/consumption/reserve values. Production rows expose actual/nominal throughput, presenter-formatted utilization, workforce, blocker, and building navigation. Market summary rows expose stock/reserve, citizen/industrial demand, incoming supply, local/recent price, report age/status, and a stable supply-chain target.

Loading, empty, and error states preserve the header/tab geometry. Errors include cause, remedy, and a semantic retry action. Status uses glyph/text/shape in addition to color. All static and dynamic content has stable `CityOverview.*` semantics.

## Reference assets

All PNGs in this directory are visual references only. They remain at generator-native dimensions and are not imported into `Content/Hansa/UI`. Each has a sibling prompt record. No resampling, cropping, production texture import, or generated full-screen shipping UI is used.

## Verification contract

Automation covers authoritative projection mapping, the six header summaries, causal target publication, unchanged-refresh suppression, virtualized-list refreshes, semantic tab selection, disabled Administration, focus order/restoration, empty/loading/error/retry behavior, long German labels, and native 1280×720 / 1920×1080 evidence metadata without post-capture resizing.
