# Hansa trade-map reference set v1

These files are exploratory screen references for the production Lübeck–Rostock route flow. They are not production-ready interactive assets and must not be shipped as full-screen raster UI.

## References

- `trade-map--route-creator--valid-draft--1672x941--v1.png`: creating and validating a route.
- `trade-map--active-route--sailing--1672x941--v1.png`: monitoring the activated Cog, cargo and destination effects.

Both references were generated at 1672 × 941 using built-in ImageGen and were not resized. Dynamic text, quantities, routes, markers, charts, buttons, focus and status are to be recreated natively in UMG/Slate. Reference values are illustrative.

## Component inventory

| Component | Production form | Required states |
| --- | --- | --- |
| Continuous top status bar | Native UMG/Slate plus approved individual icons | Default, hover, pressed, focus, disabled |
| Route/fleet list | Native virtualized list | Empty, loading, selected, active, paused, warning, error, focus |
| Regional map canvas | Native map geometry/material plus approved decorative map art | Loading, current, stale, selected city, selected route, warning |
| City marker | Native control using an individual approved ImageGen marker asset | Default, hover, selected, unavailable, stale, focus |
| Sea-route line | Native Slate/material geometry | Draft, valid, active, paused, delayed, invalid |
| Cog marker | Approved individual ship image plus native state overlays | Moored, loading, sailing, unloading, delayed, selected |
| Route inspector | Native responsive panel | Draft, valid, invalid, active, paused, warning, error |
| Stop/order row | Native control with approved good icon | Load, unload, reserve, disabled, warning, focus |
| Cargo manifest | Native rows, quantities and capacity bars | Empty, partial, full, unknown, over-capacity |
| Journey timeline | Native geometry and text | Scheduled, loading, sailing, delayed, unloading, complete |
| Warning tablet | Native shell plus approved severity icon | Notice, warning, critical, focus |
| Buttons and tooltips | Shared native Hansa actions | Default, hover, pressed, selected, disabled, focus, loading |

## MVP boundaries represented

- Lübeck and Rostock are the only active route cities.
- One Cog carries authoritative cargo.
- Orders are load, unload, quantity cap and minimum reserve.
- Preview shows time, capacity, upkeep, approximate profit and deterministic winter delay.
- Contracts, convoys, piracy, insurance, tariffs, conditional price rules and land routes are deliberately absent.

## Inspection notes

- Both images have coherent navy/linen/brass framing and preserve the map as the dominant surface.
- The create and monitor states share placement, type hierarchy, map style and ornament scale.
- Critical route state is never color-only in the references.
- Generated text is sufficiently clear for visual direction but must be replaced by native localized text.
- The maps are illustrative and must not be treated as authoritative geographic data.
- These references do not satisfy real-game MVP visual evidence; native 1280 × 720 and 1920 × 1080 captures remain required after implementation.
