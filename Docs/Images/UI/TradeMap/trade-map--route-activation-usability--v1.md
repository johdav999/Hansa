# Trade Map route activation usability

## Component inventory

| Component | Implementation | Change |
| --- | --- | --- |
| Trade Map shell | Native Slate | Unchanged |
| Filter and close navigation | Native Slate | Unchanged |
| Route list | Native Slate | Adds plain-language lifecycle, destination/time, and rival ownership |
| Map canvas | Native Slate | Unchanged |
| Simple route editor | Native Slate | Unchanged outside activation area |
| Route state card | Native Slate | New explicit heading and consequence text |
| Route activation action | Native Slate | Dynamic verb-led label and enabled state |
| Result/error feedback | Native Slate | Adds cause and remedy |

## State matrix

| State | Heading | Action | Behavior |
| --- | --- | --- | --- |
| Stopped | ROUTE STOPPED | Start route | Enabled for player-owned route |
| Active at stop | ROUTE ACTIVE | Pause route | Enabled before departure |
| Travelling | IN TRANSIT TO CITY · N TICKS | Pause available at next stop | Disabled; helper explains when pausing becomes available |
| Rival-owned | Current lifecycle plus Rival ownership | Rival route | Disabled; helper explains ownership |
| Cancelled | ROUTE CANCELLED | Route unavailable | Disabled; helper directs player to create a route |
| Focus | Existing centralized Brass focus treatment | State-specific action | Included only when action is enabled |
| Rejected | Current state remains visible | Unchanged | Plain-language cause and remedy replace generic rejection |

## Production classification

Both PNGs are visual references only. The implemented UI is native Slate with dynamic localized text, centralized styles, semantic state, and no raster imports.
