# S02-P04 automation proof visual specification

These images are reference-only. The shipping proof surface is assembled from native Slate widgets and native text; no composed image is imported as an interactive screen or production texture.

## Component inventory

| Component | Native implementation | Semantic role | Required states |
| --- | --- | --- | --- |
| Screen shell | `SBorder`/layout containers | `screen` | default, loading, error |
| Working panel | `SBorder` | `panel` | default, warning, error |
| Title and status block | `STextBlock` | `heading`, `status` | default, selected, loading |
| Primary action | `SButton` | `button` | default, hover, pressed, disabled, focus |
| Secondary focus target | `SButton` | `button` | default, hover, pressed, disabled, focus |
| Warning/error feedback | `SBorder` + `STextBlock` | `alert` | warning, error |

## Tokens and behavior

- Palette and material language follow `Docs/UIDesignBrief.md`: Baltic navy surround, warm linen work surface, brick-red primary action, muted ink text, and brass focus treatment.
- Typography, labels, status values, key prompts, and changing state are native text, never raster content.
- Focus uses a visible double brass outline and does not depend on color alone; the semantic registry also reports `focused=true`.
- Layout reserves localization expansion space and retains the 8 px spacing rhythm.
- The native proof screen supports the 1280×720 and 1920×1080 capture targets at 1:1 output pixels.

## Reference files

- `automation-proof--composed--focused-warning--1536x1024--v1.png`: hierarchy and material anchor.
- `automation-proof--working-panel--default--1536x1024--v1.png`: reusable surface reference.
- `automation-proof--secondary-control--focus--1774x887--v1.png`: keyboard/controller focus reference.

The generated references intentionally contain representative English text only. Production behavior does not use those pixels.
