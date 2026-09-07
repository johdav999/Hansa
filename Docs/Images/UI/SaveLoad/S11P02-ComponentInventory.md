# S11-P02 save/load UI component inventory

## User flow

Open the save/load ledger from the HUD menu, select a manual or autosave slot, inspect scenario/time/version/content compatibility, then save or load. Overwriting a populated manual slot and loading a save that replaces current progress require confirmation. Errors state the cause and remedy without closing the screen.

## Components

| Component | Class | Implementation | Native/reference size | States and responsibility |
| --- | --- | --- | --- | --- |
| Save/load screen shell | Native widget + screen reference | Slate modal with native brushes/material tokens | Responsive; reference 1536×1024 | Open, closed, loading, disabled background; traps focus and leaves a subdued city context visible |
| Header/navigation | Native widget | Slate text and buttons | 80 px logical height at 1080p | Title, close/back, current operation; default, hover, pressed, focus, disabled |
| Slot list | Native widget | Virtualized Slate list | 520–640 px logical width | Manual slot, rotating autosave slot, selected row, empty row, incompatible row, loading row |
| Slot row | Native widget + component reference | Slate border/layout, native status glyph | Reference 1024×1024 | Default, hover, pressed, selected, disabled, focus, empty, compatible, warning, incompatible |
| Metadata/compatibility panel | Native widget | Slate text/grid | Remaining modal width | Scenario, saved UTC/local display, game/save/simulation versions, content/registry hash summary, tick/date, authoritative checksum; empty, compatible, migrated, incompatible, corrupt |
| Action group | Native widget | Shared primary/secondary/destructive button styles | Minimum 44×44 pointer and 48×48 focus targets | Save, load, overwrite, refresh, cancel/close; default, hover, pressed, focus, disabled, loading |
| Confirmation dialog | Native widget + component reference | Slate decision panel | Bounded 640×360 logical px | Overwrite or load confirmation; confirm, cancel, focus, loading, error |
| Status/error banner | Native widget + component reference | Slate border, native icon/label/remedy | Reference 1024×1024 | Success, warning, incompatibility, corruption, I/O error, loading; never color-only |
| Focus layer | Native widget | Brass outline and semantic focus state | Scales with target | Keyboard/controller focus independent of hover; deterministic order |
| Status glyphs | Vector/SDF/native shape reference | Native text glyph/simple Slate geometry | 20–24 logical px | Check, warning triangle, incompatibility octagon, autosave rotation; always paired with text |
| Decoration | Material/native brush | Existing Hansa working/decision panels | Tiled/procedural | Restrained linen, oak, brass and ink; never intercepts input |

No chart or production raster is required. Slot data is virtualized/native and UI/transient state is excluded from authoritative saves.

## State matrix

| State | Treatment |
| --- | --- |
| Default | Linen row, Ink text, Oak rule |
| Hover | Brass edge without geometry movement |
| Pressed | Existing shared pressed padding/tone |
| Selected | Persistent Brass outline, selection label and marker |
| Disabled | Muted Ink plus a cause/remedy tooltip or adjacent status |
| Keyboard/controller focus | High-contrast Brass focus ring, semantic `focused=true` |
| Loading | Stable layout, operation label, disabled conflicting actions |
| Warning/migration | Warning Amber, triangle glyph and migration explanation |
| Error/incompatible/corrupt | Oxblood edge, octagon/exclamation glyph, cause and remedy |
| Success | Prosperity Teal, check glyph and explicit success text |

## Layout and accessibility

- Maximum modal width 1500 px at 1920×1080, centered with 24–32 px gutters; collapses to a single-column details region at 1280×720.
- Uses the 8 px spacing scale, 16/24 px panel padding, H1/H2/Body/Data/Caption typography tokens, tabular numerals, and 80–140% UI scale.
- Long German labels and slot names have 40% expansion space and wrap without hiding actions.
- Status always uses icon/shape plus text. Essential text meets 4.5:1 contrast and focus/icons meet 3:1.
- Escape/B cancels confirmation first, then closes. Enter/A activates the focused enabled action. Focus returns to the HUD save/load button.
- No animation is required; operation progress updates through state changes and respects reduced motion.

## Image-generation plan

1. Composed screen style anchor: complete selected compatible manual-slot state, 1536×1024, reference only.
2. Slot-row component reference: selected compatible row, 1024×1024 canvas, reference only.
3. Confirmation-dialog component reference: overwrite confirmation, 1024×1024 canvas, reference only.
4. Status-banner component reference: incompatible/corrupt state, 1024×1024 canvas, reference only.

The references define hierarchy and material restraint. Shipping implementation uses native Slate and shared Hansa style tokens.
