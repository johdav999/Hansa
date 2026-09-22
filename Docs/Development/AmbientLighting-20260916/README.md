# Ambient illumination calibration — 2026-09-16

Request: brighten shaded building faces while retaining the existing sun angle and exposure.

## Result

The gameplay Sky Light uses Engine/EngineResources/GrayLightTextureCube. Its former 3.6 intensity was too weak against the approximately 15,000-lux sun at the existing fixed exposure. Direct runtime comparisons used the same camera, sun and exposure. Intensity 30 and 100 produced little improvement; 1,000 visibly opened the shaded brickwork; 2,000 was selected for readable walls while retaining directional shadows.

The shared lighting curve now interpolates from the unchanged night floor 0.65 to daylight 2,000. The lighting-state default and existing curve/capture assertions were updated. Sun orientation, solar intensity, source angle, exposure, materials, ambient occlusion and cubemap are unchanged by this revision. This value is specific to the existing gray cubemap and exposure; it is not a general-purpose skylight recommendation. Editor captured-sky preview settings are separate and were not changed in this revision.

## Verification

- HansaEditor Development build succeeded (Saved/BuildArtifacts/20260916-184424566-build-HansaEditor-Win64-Development).
- Six Hansa.World.LubeckArt headless regression tests passed (Saved/BuildArtifacts/20260916-184501219-automation-Hansa.World.LubeckArt). Render-only tests were not run by this headless command.
- Fresh PIE after rebuilding reported Sky Light intensity 2000, sun pitch -60 degrees and sun intensity 15105.4873. Runtime lighting remains enabled in the rebuilt game.
- Visually inspected fixed-camera runtime comparisons. Screenshot captures below are native engine captures, not generated imagery. No raster assets or GUI styling changed.
- Temporary global skylight multiplier reset to 1. Temporary camera restrictions and paused lighting tick existed only in the diagnostic PIE session, which was stopped.

## Evidence

- [Before: intensity 3.6](before-3.6.png)
- [Comparison: intensity 1000](comparison-1000.png)
- [Selected: intensity 2000](after-2000.png)

The additional ambient light also raises shaded ground and contributes somewhat to sunlit surfaces. Directional shadows remain visible. Future changes to the ambient cubemap require recalibration.