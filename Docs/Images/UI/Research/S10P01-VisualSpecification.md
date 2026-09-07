# S10-P01 Research UI visual specification

## Component inventory

| Component | Runtime / editor role | Shipping implementation | Required states |
| --- | --- | --- | --- |
| Screen shell | Full-screen research workspace with title, close action, branch chart, detail and queue areas | Native Slate layout and centralized Hansa style tokens | default, loading, warning, error |
| Branch chart | Three bounded columns: Commerce, Production and Logistics | Native Slate panels and graph connectors | default, selected branch, disabled |
| Technology card | Stable node summary with title, prerequisite state, time/cost and effect category | Native Slate button/card; reference-only raster | available, hover, pressed, selected, locked, completed, controller focus, warning |
| Prerequisite connector | Dependency direction and completion state | Native Slate/SDF line and arrow geometry | locked, available, completed, warning |
| Technology detail | Selected technology explanation, prerequisites and deterministic effects | Native text, icons and lists | default, locked, affordable, unaffordable, completed |
| Queue/progress strip | Persistent single-item queue, progress, remaining ticks and cancel/close affordances | Native progress bar, text and buttons; reference-only raster | empty, researching, completed, warning |
| Status/feedback | Contextual rejection, invalid graph and completion feedback | Native inline status/alert | notice, warning, error, success |
| Authoring graph shell | Specialized editor view with graph canvas, details and validation findings | Native Slate editor panel | valid, missing reference, cycle, unreachable node, focus |

## Interaction and accessibility contract

- Controller order is close, branch headings, technology cards in branch/dependency order, queue, then the primary research action.
- Focus uses a two-pixel sky-blue outline plus a non-color cue. Selection uses an inset marker and `Selected` label in semantic state.
- Locked cards expose every missing prerequisite and keep the unlock/effect explanation visible.
- Status is never communicated by color alone: cards use icons, labels and border patterns.
- Dynamic names, prices, tick counts, progress and validation messages are native text and are never baked into raster assets.
- Layout reserves at least 35 percent text expansion and switches to a scrollable single-column branch view at compact widths.
- Body text targets WCAG 2.1 AA 4.5:1; large labels and essential icons target 3:1.

## Visual system

- Use the established navy, parchment, ink, muted brass, amber warning and sky-blue focus tokens from `Docs/UIDesignBrief.md`.
- Paper and ledger materials are restrained flat backings; graph geometry remains crisp and scalable.
- Historical character comes from ledger ruling, brass dividers and small wax-seal completion marks, not decorative clutter.
- All generated images in this folder are visual references only. The runtime and Authoring Studio surfaces are reconstructed with native Slate widgets.

