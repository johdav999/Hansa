# Construction progression reference

Status: proposed visual reference only; no gameplay implementation or production import.

## Component inventory before generation
- Shell: reuse current navy/brass construction tray and corner decoration; native layout/frame.
- Navigation: reuse existing category buttons and demolition control; add one reusable text-only tier selector with Day Laborers, Craftsmen, Merchants.
- List: reuse existing illustrated product cards (Beer, Bread, Firewood, Fresh fish, Planks) for direct screenshot comparison.
- Panels/controls: existing chain drawer, building cards and tooltips remain unchanged and are not expanded in this reference.
- Feedback: native selection, focus, locking explanation and placement feedback. No charts or new world overlays.
- Icons/decorative imagery: reuse approved category/goods artwork during implementation. No new production icons are requested.

## State specification
Default: slate surface, Chalk text. Hover: subtle brighter slate without resizing. Pressed: darkened fill. Selected: brass outline and underline. Keyboard/controller focus: independent high-contrast outer ring. Disabled building action: reduced emphasis plus explicit unmet requirement. Locked tiers remain browsable for preview. Loading: stable card geometry and explicit Loading text. Warning: amber with reason. Error: oxblood with reason. These states are native; no separate raster variants required.

## Proposed behavior
Keep category and tier as independent selections. Switching category preserves the selected tier. Switching tier changes the category contents; it does not change the player rank. Three progression levels only. Merchant-house founding is a separate business milestone, and Ratsherr is a political office. Unlock tier and required worker tier must be separate concepts. Shared infrastructure should remain reachable at every unlocked tier. Production chains must keep prerequisite buildings accessible even when a different tier first unlocks the end product. Goods shown in this image are retained from the user screenshot; their exact tier assignments are not approved by this mockup.

## Deliverables and constraints
Generate a composed reference and a separate reference for the only new reusable visual component: tier selector. Existing shell, category controls, goods cards and icons are reused, not redesigned. Target composed canvas 1536x640; target component canvas 1536x640, allowing tool-native size if different. No resampling. Save actual native dimensions in filenames and sibling prompt records. All imagery is reference-only, never a flattened shipping GUI. Implementation would use native Slate/UMG text, layout, filtering and state treatments with existing approved ImageGen icons.

## Approved implementation

The user approved this reference and requested in-game implementation on 2026-09-17. See ../../../Development/ConstructionProgression.md for actual filtering, compatibility and validation. The generated images remain reference-only.
