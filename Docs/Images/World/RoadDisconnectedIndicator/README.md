# Road-disconnection indicator captures

These are native Unreal gameplay captures of the accepted world-space 3D road-disconnection marker. No screenshot was resized.

| Resolution | Disconnected | Reconnected |
|---|---|---|
| 1280 x 720 | ![Disconnected 1280x720](road-disconnected-1280x720.png) | ![Reconnected 1280x720](road-reconnected-1280x720.png) |
| 1920 x 1080 | ![Disconnected 1920x1080](road-disconnected-1920x1080.png) | ![Reconnected 1920x1080](road-reconnected-1920x1080.png) |

The capture test removes the bakery's adjacent road segments through gameplay commands, asserts that authoritative market access is lost and the imported marker is visible, then rebuilds the road, advances construction, asserts access recovery, and verifies the marker is hidden.
