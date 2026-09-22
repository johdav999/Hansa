# Cog selection marker

The Cog previously only enabled custom depth when selected. It now has a dedicated
non-colliding marker mesh attached to its waterline root, with a segmented brass
oval, chalk inlay and bow diamond. `SetSelected` toggles the marker only for visible
sea vehicles. `ClearProjection` clears it before pooling/reuse. Cargo manager
sampling already updates location, yaw and selection together.

Source and generation evidence: `SourceArt/Generated/Props/CogSelectionMarker/`.
Runtime mesh: `/Game/Mesh/cog-selection-marker/SM_CogSelectionMarker`.
The reflected component exposes standard Blueprint mesh/material preview and
picker controls. No gameplay definition, stable ID, simulation rule, serialized
save field or provider dependency changes. Existing cargo inspector semantics are
the selection automation interface.

The existing `Hansa.UI.ShipNavigation.RealViewport` test additionally checks the
imported marker, no collision, attachment after sailing, and deselection. Native
selected, ordered, arrived, home and deselected captures are retained with results.

Verification status is recorded in `SourceArt/Generated/Props/CogSelectionMarker/evidence/`.
ImageGen output is reference-only; the imported mesh and three materials are the
runtime assets. Shipping packaging and exhaustive all-map/zoom coverage are not
claimed by this focused feature verification.

## Final verified result

- Main Hansa Development Editor build passed after a normal clean shutdown released loaded DLLs. Editor reopened on the original terrain map and camera restored.
- Three navigation regression tests passed.
- Real mouse selection, right-click sailing, return and deselection passed at 1280x720 and 1920x1080 on the current surveyed map using the main binaries.
- Native selected and deselected captures were inspected. Brass/chalk ring and bow diamond remain readable on the blue water and disappear after deselection. Hull materials are preserved.
- Water correction: use translucent unlit overlay materials with depth testing disabled and inverse exposure compensation. The ring remains visible through occluding surfaces intentionally; it is a selection overlay, not a physical object. No full Shipping package or exhaustive zoom/map sweep was run.
- Final evidence: evidence/verification.json. Final native captures: Docs/Images/UI/CogSelection/ship-1280x720-*.png and ship-1920x1080-*.png.
- Reproduction: build_marker.py -- 4, verify_exports.py, import_mcp.py (new destination only), fix_exposure_mcp.py, fix_water_overlay_mcp.py. The latter two scripts are required engine-specific material reconstruction steps.
