# EMVP-P27 — Research screen integration

Implemented 2026-09-09. Open the normal game HUD's **Research** action (or run `Scripts/LaunchGuiPreview.ps1`, then Research). This completes the bounded three-branch MVP research workflow; it is not a blanket AAA certification for the entire game.

## Behavior

- Commerce, Production and Logistics use the nine compiled MVP technologies, ordered by prerequisites within each branch. All three branches are visible at wide layouts; compact layouts expose branch tabs and independently scrollable selected details.
- Cards compare names, state, point cost, duration and strategic effect. Native dependency lines and named prerequisite controls expose the relationship and navigate to the required technology.
- The selected detail separates **On completion** from **Applied effects**. Applied text comes from authoritative applied-effect records, not inferred completion flags or a widget-side bonus calculation.
- The persistent queue shows the actual one-slot capacity, available research points, active technology, tick progress and remaining simulation ticks. The game clock controls progress; pause is explicitly explained. The primary action and locked reason stay outside the detail scroller.
- Queue eligibility calls the read-only `FHansaResearchExecutor::CanQueue` check also used by `TryQueue`. The existing typed runtime command remains the only mutation path. No cost or rule is reimplemented in Slate.
- Effect actions resolve stable targets to the Lübeck market, a matching placed production/storage building owned by the researching house, or its existing non-cancelled affected route. Missing world targets report that no affected building or route is present. Building links select/frame the actual world building; route links select the route in the existing trade map.
- Loading, rejected submission, insufficient points, missing prerequisites, occupied queue, available, researching and completed states have explicit text. Native shared actions retain hover, pressed, selected, disabled and keyboard/controller focus rendering. Submission rejects reentry while busy. Back restores the HUD opener.
- Semantics expose nodes, prerequisites, effects, selected detail, loading/error feedback, applied effects and active queue progress. Widget bounds and scroll clipping are reported in physical pixels, including compact branch visibility.

## Components and visual references

| Component | Implementation / states |
| --- | --- |
| Bounded screen shell | Native linen surface and navy header; maximum 1600 logical units wide, 720 high, constrained by viewport safe area |
| Navigation | Close, compact branch tabs, deterministic technology/dependency/effect/action focus order |
| Branch chart and lists | Three prerequisite-ordered native lanes, native connector geometry, scroll containers |
| Technology cards | Shared native actions, selected/focus/hover/pressed; explicit locked, available, researching, completed text |
| Selected details and controls | Native text, prerequisite actions, causal target actions, pinned start action and blocked reason |
| Queue and feedback | Persistent navy strip, native progress bar, points, remaining ticks, loading/error text |
| Icons / decoration | Existing shared focus/state system and native connector geometry; no new decorative raster |

P27 reuses the existing approved ImageGen family in `Docs/Images/UI/Research/`. No missing research-specific component required another generation call, and no raster was imported into production content. All game controls and text are native Slate.

Reused reference masters and their final prompt records:

- `research--composed--selected-active--1536x1024--v1.png` — 1536 × 1024; built-in ImageGen; sibling `.prompt.md`.
- `research--technology-card--selected-focus--1145x1374--v2.png` — 1145 × 1374; built-in ImageGen; sibling `.prompt.md`.
- `research--queue-strip--active-focus--1975x796--v2.png` — 1975 × 796; built-in ImageGen; sibling `.prompt.md`.

The composed reference was inspected at original resolution. Native captures were compared for bounded branch hierarchy, dependency ordering, selected detail, palette, focus and persistent queue. Invented technology names, unsupported cancel controls, painted icons and wax seals in the reference are not imported gameplay features. The existing queue reference's opaque backing remains reference-only. No generated image was stretched or resampled.

## Verification

- Development editor build: passed (`20260909-135239550-build-HansaEditor-Win64-Development`).
- `Hansa.UI.Research`: three tests passed (`20260909-135303980-automation-Hansa.UI.Research`) and cover root/controller focus, branch/tab semantic parity, disabled-action exclusion, eligibility parity, command rejection/recovery, stable causal targets, insufficient points, real save codec progress and completed-effect round trips.
- `Hansa.Content.Research`: the authored nine-technology catalogue test passed (`20260909-134650246-automation-Hansa.Content.Research`).
- `Hansa.Simulation.Research`: three tests passed, including graph diagnostics and authoritative command/tick effects.
- `Hansa.Integration.Save`: nine tests passed, including existing format migrations. P27 changes neither the save format nor gameplay definition schema.
- Native `Hansa.UI.Research.RealViewport`: **12 successful runs, 120 original-size PNGs and matching semantic TSVs**, covering 1280×720, 1920×1080, 2560×1440 and 3440×1440, each at 80%, 100% and 140% UI scale.
- Each native run opens Research from the HUD with controller input, follows a prerequisite, starts research, completes it on the normal game clock, and opens market/building/route causal targets. All 12 runs verify the visible pinned primary target is at least 48 physical pixels high (one-pixel geometry rounding tolerance).
- Large text, high contrast and reduced motion are exercised in every profile. Loading and error captures are explicitly named **presentation fixtures**; those two states are injected for inspection, not claimed as naturally occurring backend failures. Backend rejection behavior is independently tested.
- Shipping build and executable/receipt exclusion audit passed (`20260909-135346313-shipping-exclusion-Win64/result.json`). This is not a full cooked depot/package certification.
- Scoped `git diff --check`: passed.

Run `python Scripts/ValidateResearchPresentation.py` after native captures to verify dimensions, semantic progress/completion, loading/error feedback and primary-action bounds. The script copies the originals without resampling and records SHA-256 hashes in `Docs/Images/UI/ResearchP27/verification.json`.

Open [the original-size comparison viewer](../Images/UI/ResearchP27/comparison.html) to inspect all final native captures against the existing composed reference. Native evidence is under `Docs/Images/UI/ResearchP27/Native/`; these files are QA evidence, not imported production textures.

## Inspection fixes and limits

Native inspection caught and corrected prerequisite ordering, a scrolling primary action, a missing-font dependency arrow, clipped locked reasons, excessive screen height on large displays, zero-sized research semantic bounds, and the shared action disabled-state contract. Disabled research actions now label their actual state, and submission restores focus to the selected card. Compact high-accessibility layouts intentionally scroll long details while keeping the locked reason, primary action and queue visible.

The current MVP exposes one research slot and simulation-tick durations. It does not add cancellation, speculative multi-item queues, new research branches, new research data, provider integrations or a second visual system. Presentation tests and capture checks do not replace human release sign-off, localized copy review or full Shipping package validation.
