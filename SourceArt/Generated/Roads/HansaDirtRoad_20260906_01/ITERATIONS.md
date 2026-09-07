# Render / inspect / correct log

1. Source v1 inspected in v1_Kit.jpg and v1_Surface.jpg. Crown and narrow wear bands looked too sculpted versus the photograph. Reduced crown from 9 to 4.5 cm and rut depression from 5.5 to 2.5 cm; widened rut Gaussian. Rerendered v2; inspected v2_Surface.jpg and confirmed softer physical profile.
2. Dark shoulders remained too continuous and graphic. Reduced shoulder color attenuation from 43% to 24%, preserving geometry. Rerendered v3 and inspected v3_Kit.jpg; edges read as soil variation rather than dark borders.
3. Wheel wear still read as ruler-straight bright bands at kit distance. Added small endpoint-faded track wander and reduced brightening from 8% to 3.5%. Rerendered v4, inspected export/reimport and raking views; wear is restrained and less uniform.
4. Export correction: GLB tangent warning on quads. Added temporary export-only triangulation, regenerated both formats, reimported both in clean contexts and rendered kit/material views. Editable master retains quad topology.
5. Engine correction: first actual Unreal capture showed detached spline components and poor framing. Read component transforms, fixed incompatible mobility/reparenting in the saved Blueprint, refreshed only the isolated review actor. Reopened map, read component starts/ends and measured zero endpoint/tangent error. Improved camera and hid editor helpers for final captures.
6. Engine micro-relief reduced from XY 0.5 to 0.08. New actual capture inspected: some grain harshness remains, so mip/streaming visual acceptance stays open. No false claim that this correction eliminates the remaining non-power-of-two base-color aliasing.

Failed setup runs are not review cycles: Blender 3.5 UV-layer handle invalidation initially corrupted loop indices. Reacquiring UV layer after creating the color layer fixed it; final source uses validator assertions. Failed material wiring was inspected before resuming owned staging assets; no unrelated asset was overwritten.
