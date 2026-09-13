# Economy diagnostics

Runtime economy snapshots are written to Saved/Logs/Hansa.log with the prefix
[EconomyDebug], every 10 wall-clock seconds, including while simulation speed is
paused. The first real-time update emits a baseline snapshot.

Set hansa.Debug.EconomyLogInterval to another interval in seconds, or 0 to disable.
This is runtime-host diagnostic state; it does not change simulation or save data.

- Population: city residents, housing capacity, laborer/artisan residents, workforce
  supplied, assigned, and available.
- MarketStock: exact city inventory and building identity, good, total stock,
  reservations, and unreserved available stock. Accepted goods with zero stock
  are included.
- Production: building/recipe, input/output inventories, active state, exact blocker,
  missing good and quantities, staffing, batch progress, cumulative batches and
  batches completed since the previous snapshot. baseline=1 means no earlier
  production sample exists.
- Output: actual output during the latest simulation tick and nominal batch output.
- Residence/Consumption: residents, workforce, supply inventory, physical Market
  access, current demand/consumption, access/affordability/reliability, and rolling
  consumption totals with their recorded duration.

Quantities are game units (raw milli-units divided by 1000). BP fields use 10000
for 100%. Last-tick values are snapshots, not ten-second totals: batch production
can legitimately show zero on a sampled tick. Use cyclesSinceLog to detect activity
between samples. Rolling consumption is up to 30 game days, not current stock.

Investigation on 2026-09-12: the live DebugGame clock continued advancing beyond
day 70 with no runtime tick-failure entry. The editor closed cleanly before actual
Market stock could be inspected. A 76-day two-residence regression with available
bread passes, including full rolling-history fulfillment. This does not establish
the cause of the user's specific unavailable-consumption state; capture these
logs on the next reproduction.
