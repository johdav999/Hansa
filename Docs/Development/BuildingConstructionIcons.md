# Construction building icons — 2026-09-10

## Cause and component inventory

SHansaBuildCardButton mapped Farm, Mill and Bakery explicitly and otherwise used
the residence icon. Road, Warehouse and Dock now have explicit correct mappings.
The existing category-navigation icons remain Road, Storage (crates), Harbor
(anchor); these are different controls from the building buttons above them.

Components: existing 48x48 native building action, generated subject artwork,
native tooltip and native state/focus outline. Existing shell/navigation/layout
are retained. No tables, charts or new decoration are introduced. Default,
hover, pressed, selected, disabled, keyboard/controller focus, warning and error
are supplied by SHansaAction. Loading has no new artwork. No dynamic labels are
baked into images.

## Assets and prompts

- Road: reuse approved ImageGen road master and existing Road display variants.
  SourceArt/UI/Icons/icons--road--default--1343x1171--v1.png and sibling prompt.
- Warehouse: new built-in ImageGen, 1254x1254 original RGBA master:
  SourceArt/UI/Icons/icons--warehouse--default--1254x1254--v1.png.
- Dock: new built-in ImageGen, 1254x1254 original RGBA master:
  SourceArt/UI/Icons/icons--dock--default--1254x1254--v1.png.
- Exact final prompts, alpha crops, dimensions and revision notes are in each
  sibling .prompt.md and SourceArt/UI/Icons/manifest.json.
- Production display PNGs: Content/Hansa/UI/Icons/Warehouse--SIZE.png and
  Dock--SIZE.png for 16,20,24,28,32,40,48,56,64,80,96,112,160 square pixels.
  These use the existing Slate dynamic-image brush loader and UFS packaging
  wildcard, not imported UTexture assets. Road uses existing Road--SIZE.png.

Requested size was 48x48. The generator produced 1254x1254; original masters are
preserved. Display copies use transparent-margin cropping and premultiplied-alpha
Lanczos proportional fit with 2px padding, under the user's GUI resizing exception.
Scripts/PrepareConstructionBuildingIcons.py documents and reproduces processing.
No artwork was manually redrawn. Existing icon-family prompt tokens, upper-left
light and three-quarter view are repeated in the new prompts.

## Inspection and verification

Original generated images inspected: warehouse cargo doors/crates/hoist and dock
pier/crane/water form distinct silhouettes; no text or backdrop. RGBA transparency
verified numerically. Display variants reviewed at 32,40,48,64 pixels against the
intended navy background, including sizes below the 48px default. Review artifact:
Docs/Images/UI/Construction/BuildingIcons/display-size-review.png (diagnostic only).

Development editor build succeeded. The BuildMenu suite has an existing stale
SemanticsShortcutsAndFocus assertion requiring the removed secondary action row.
The artwork change does not restore those intentionally removed controls.
Native capture coverage now includes Road, Warehouse and Dock stages.

## Market building found during native QA

Storage contains both Market and Warehouse. A fourth explicit mapping was added:
Building.Market uses an individually generated market-stall image, not Civic's
town-hall image. New 1254x1254 original RGBA source and sibling prompt:
SourceArt/UI/Icons/icons--market--default--1254x1254--v1.png.
Production copies use Content/Hansa/UI/Icons/Market--SIZE.png for the same 13 sizes.
The final display-size review includes all four subjects; at 32/40/48/64 pixels
the market awning, warehouse cargo doors, dock crane and road stones are distinct.

Final editor build and Hansa.UI.BuildMenu.StableCardFocusAndChainEdges passed.
The broader menu-suite failure noted above concerns the removed action row and
was not introduced as a change to its controls by this icon task.

Final native verification: Hansa.UI.Construction.RealViewport passed at 1280x720
and 1920x1080 after the Market mapping was included. All three trays were visually
inspected as unscaled crops in Docs/Images/UI/Construction/BuildingIcons/native-tray-review.png.
Road shows cobbles; Storage shows market stall plus warehouse; Harbor shows dock
and crane. No residence fallback remains in those trays, no icon clipping or
background fringe was observed, and existing selected category outlines remain.
Full captures: Saved/P22/tray-RESOLUTION-road-icon.png, -warehouse-icon.png,
-dock-icon.png. Final logs: Saved/BuildArtifacts/20260910-214006007-building-icons-final-1280-720
and 20260910-214030357-building-icons-final-1920-1080.
Production assets are the individual Content PNGs; review PNGs are diagnostics only.
