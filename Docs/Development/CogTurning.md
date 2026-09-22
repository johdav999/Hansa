# Smooth Cog turning

The cargo presenter now eases sea-vessel yaw along the shortest angular arc instead of assigning each path segment heading immediately. A 90-degree turn takes 1.5 simulation ticks with smooth acceleration and deceleration. Position remains the existing authoritative path projection; this does not add hull physics or change navigation clearance.

Heading uses simulation tick plus presentation fraction, so pause freezes a turn and game speed scales it. Repeated samples cannot advance rotation. Mid-turn orders retain the current orientation; arrival can finish the pending turn. Pooled identities and clock rollback discard obsolete turn state. Route berth headings use the same interpolation rather than resetting yaw on every snapshot. Land wagon headings retain their existing behavior.

Regression coverage: Hansa.World.Vehicles.SmoothCogHeading checks intermediate angles, pause, render-frame independence, arrival, reversal, mid-turn retargeting, angle wraparound and clock rollback. Hansa.World.Vehicles.ReadOnlyContract checks the unchanged cargo presentation contract.

No gameplay schema, save format, editor schema or generated asset changes are required. Existing Cog artwork is reused.

## Verification — 2026-09-17

- UE 5.8 Development Editor build passed in Saved/CogTurningVerify.
- Hansa.World.Vehicles.SmoothCogHeading: 1/1 passed, including command-tick clock rebasing.
- Hansa.World.Vehicles.ReadOnlyContract: 1/1 passed.
- Hansa.ShipNavigation: 3/3 passed against the final concurrent navigation continuity correction.
- Build log: Saved/CogTurningVerify/build.log. Per-filter logs/results: Saved/CogTurningVerify/Saved/BuildArtifacts/.
- An initial broad Vehicles run also selected four visual capture tests requiring the P19 review map; those prerequisites were absent. The exact headless regression filters above passed subsequently.
- No new real-viewport visual assessment was performed. The running main editor was preserved; rebuild/restart it to load the changed C++.

Command-driven simulation ticks explicitly rebase the turn clock without advancing its angle, so issuing orders cannot skip ahead within an ongoing turn.
