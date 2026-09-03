# S02-P01 Authoring Studio visual specification

## Deliverable classification

- The composed screen and component images are **reference-only rasters**.
- The interactive screen is a **native Slate widget** hosted by the Editor-only `HansaEditor` module.
- No generated raster is imported into `Content/` or referenced by the implementation.
- Standard Unreal Editor typography and control glyphs remain native; all changing text and values remain native.

## Screen component inventory

| Component | Implementation | Responsibility |
| --- | --- | --- |
| Studio shell | Native Slate dock tab | Bounded three-column workspace with a validation footer |
| Navigation toolbar | Native Slate toolbar/controls | Screen title, schema export action, validation action, undo and redo |
| Definition browser | Native virtualized `SListView` | Discovered definition assets, selection, empty state and search filtering |
| Search control | Native `SSearchBox` | Case-insensitive asset/class/definition-ID filtering |
| Details inspector | Native Property Editor `IDetailsView` | Automatic reflected-property editing with transactions and undo/redo |
| Validation results | Native virtualized `SListView` | Severity icon/text, field path, cause and remedy |
| Status/feedback | Native text, icon and semantic color | Empty, valid, warning and error summaries without color-only meaning |
| Controls | Native Editor buttons | Validate, export schema, undo and redo with enabled/disabled states |
| Icons | Built-in Editor glyphs/native shapes | Search, validation severity, undo/redo and asset identity |
| Charts/overlays | None in S02-P01 | No visualization is required for the generic schema foundation |
| Decorative imagery | None | Historical ornament is outside the Authoring Studio MVP shell |

## Layout contract at the 1536 × 1024 reference size

- 48 px-equivalent toolbar at top.
- 280–320 px definition browser at left.
- Flexible details inspector in the center.
- 340–400 px validation/context panel at right.
- 180–240 px validation results footer spanning the workspace.
- 8 px spacing scale, 16 px panel padding and minimum 40 px pointer targets.
- Selection and keyboard focus use a brass outline; focus is independent from hover.
- Palette follows `Docs/UIDesignBrief.md`: Baltic Navy, Harbor Slate, Linen, Ink, Brass, Prosperity Teal, Warning Amber and Oxblood.
- Materials are restrained flat Slate fills with only subtle linen/timber suggestions in the references.

## Component states

| Component | Required states |
| --- | --- |
| Toolbar button | Default, hover, pressed, disabled, keyboard focus |
| Search | Empty, active text, no matches, keyboard focus |
| Definition row | Default, hover, selected, disabled/unavailable, keyboard focus |
| Details field | Default, hover, editing, invalid, disabled/read-only, keyboard focus |
| Validation row | Informational, warning, error, selected, keyboard focus |
| Workspace | Empty, loading, valid/success, warning, error |

Every warning/error state combines icon, label and color. Stable geometry prevents layout jumps between empty, valid and invalid states. The primary workflow is fully keyboard reachable; localization expansion is supported by flexible labels and bounded columns rather than fixed English widths.

## Reference generation set

1. Composed Authoring Studio workspace reference, 1536 × 1024, 3:2.
2. Definition browser row/search component reference, square native generator output.
3. Reflected details field-group component reference, square native generator output.
4. Validation result row component reference, square native generator output.
5. Toolbar action/focus component reference, square native generator output.

All outputs are inspected at their generated native size and saved without resizing. Matching `.prompt.md` records identify the built-in generation mode and reference-only status.
