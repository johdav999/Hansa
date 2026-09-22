# Alert panel redesign

Design authority: Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md (user confirmed).

## Component inventory and states
- Shell/header: native Slate navy/brass frame, title/count and collapse action; expanded/collapsed, focus, hover, pressed. Target width 320/336 Slate units, viewport-bounded scrolling.
- Alert card: native linen surface, category/count, severity icon and label, affected object, age, problem and causal tooltip. Notice/warning/critical, empty, stable updates; no loading animation.
- Actions: existing shared SHansaAction; Details, Locate, Snooze, Pin. Default/hover/pressed/focus/disabled/selected use shared native styles. Localized text and 48-unit targets.
- Group overflow: native action opens remaining alerts, with explicit count; maximum three expanded cards.
- Pinned and snoozed rows: shared native surfaces and controls.
- Icons: reuse approved individual ImageGen Information/Warning/Error artwork through SHansaGlyph.
- Charts/overlays/decorative imagery: none needed.

New ImageGen outputs are screen, shell and card references only; existing shared button and icon references are reused. Shipping output is native Slate, never a screen bitmap.

## Update contract
Age/evidence/text changes update existing widgets. Structural changes may reconcile list contents while preserving scroll/focus. No continuous flashing or animation. Stable severity and typography, opaque operational data surfaces.

Implementation, evidence and remaining limits: ../../../Development/AlertPanel.md


