# Starting Cog and water exploration — 2026-09-17

The player requests a visible starting Cog near the initial waterfront, selectable ship details, and right-click sailing throughout the map. This explicitly extends the old route-only/river-navigation MVP boundary.

## Player flow

New Game places the existing owned Cog at the nearest surveyed water cell with room for its hull. Left-click selects it. The existing cargo inspector shows state, owner, capacity, upkeep and sailing instructions. Right-click connected open water to sail; right-drag still pans, Ctrl/right-drag rotates, and G issues an order at the pointer. Return to berth and Stop ship reuse native inspector actions and semantic/controller focus. Game pause/speed governs travel. Unreachable water and land orders preserve the current course and give a remedy. Disconnected lakes and channels narrower than the hull cannot be reached from this starting waterbody.

## Authority, authoring and migration

`FHansaMoveShipCommand` uses the normal authority gateway (schema 8), verifies ownership and route state, and computes a deterministic cardinal path over the full immutable authored placement topology. No camera or streamed collision boundary limits pathfinding. Four metres of lateral shoreline clearance accommodates the existing 7.81-metre beam; the starting berth additionally requires twelve metres of turning room; movement is one four-metre cell per tick. This is a fixed simulation rule, not a new independently authored asset or duplicate map. Editor vehicle metadata explains the same mode and upkeep semantics; shared navigation validation rejects invalid restore/start records. Water material/surface queries affect height only, never movement authority.

The existing stable vehicle and inventory identity is preserved. Manual exploration and active trade routes are mutually exclusive; return home before activation/reassignment. Save v10 and fingerprint v23 include navigation city, home/current cells, ordered path and cursor. Old format 9 / fingerprint 22 saves verify their original hash and retain their old berth mode rather than relocating an existing voyage. No provider integration or generated media is introduced. Existing approved Cog assets are reused. Automation vehicle queries include current/home water cells and movement state; normal typed commands and inspector semantics are the controlled entry points.

## Component inventory and states

All components reuse the existing cargo inspector and approved native system: HUD shell/navigation; identity panel; labeled ship detail rows; journey/status feedback; shared Frame, related view and Pin controls; shared native Return to berth and Stop ship actions; existing world selection outline; unchanged Cog mesh/rig/sails. There are no new decorative images, charts, icons, screen shell or raster assets. Native controls retain default/hover/pressed/focus/disabled treatment. Ship states are anchored, sailing, active trade and rejected order; errors use explicit wording. Dynamic data remains native Slate. This is functional extension of the approved inspector design, not new artwork; no ImageGen generation or asset promotion applies.

## Verification

`Hansa.ShipNavigation` covers island avoidance, deterministic paths, hull clearance, unreachable water, ownership, startup distance, invalid-order rollback, trade-route compatibility and in-flight save/restore continuation. `Hansa.UI.ShipNavigation.RealViewport` uses actual Slate mouse input to select the Cog and issue a right-click order, waits for arrival under the ordinary game clock, uses the focusable return action, and captures native viewport evidence. Results and limitations are recorded after execution below.

### Executed evidence

- UE 5.8 Development Editor compiled successfully in the isolated `Saved/ShipNavigationVerify` project. Its Content/Docs/SourceArt junctions use the main project assets. The running main editor was preserved; it requires a rebuild/restart to load the final C++ changes.
- `Hansa.ShipNavigation`: 2/2 pass, including a connected destination over one kilometre from the start, route exclusion, deterministic in-flight restoration and island avoidance.
- `Hansa.Integration.Save`: 13/13 pass, including legacy migration and current envelope integrity.
- `Hansa.UI.ShipNavigation.RealViewport`: pass at 1280x720 and 1920x1080 using real mouse selection/orders and the ordinary simulation clock. Native screenshots were inspected without resizing: Cog, sail state, water position, details, feedback and native actions are readable; at 720p the action area scrolls.
- Compact reports: `Docs/Development/ShipNavigationEvidence/`. Native selected/ordered/arrived/home captures: `Docs/Images/UI/ShipNavigation/ship-1280x720-*.png` and `ship-1920x1080-*.png`, with matching state snapshots. These are verification references, not imported production art. No assets or prompt sets were generated.

Navigation uses surveyed water cells and beam clearance. It does not cross disconnected lakes, land, or insufficiently wide channels. It is grid navigation, not a full hull/turning-radius physics simulation. New Game receives free navigation; migrated old saves retain their prior route berths. Exploration must return to its home berth before an intercity trading route is activated.

Shipping Win64 also compiled successfully, and the repository Shipping exclusion audit passed. Evidence: Docs/Development/ShipNavigationEvidence/shipping-result.json.
