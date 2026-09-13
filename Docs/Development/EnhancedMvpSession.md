# EMVP-P28 — Session, onboarding, pause and results

Implemented 2026-09-09. Launch the game normally (or use `Scripts/LaunchGuiPreview.ps1`). The opening dossier offers **Begin your city**, optional contextual help and save/load. During play, use **Menu**, Escape when no tool/panel needs closing, or the controller Menu button. **Review prosperity** opens the current authoritative goals. This is the bounded P28 session implementation; frontend/new-game/settings work belongs to P29.

## Behavior and controls

- Opening and pause hold the authoritative simulation clock. Begin starts normal play; closing an ordinary pause restores the previous speed. A loaded game stays paused until the player resumes. A full viewport input barrier blocks clicks into underlying city controls while the session dossier is open.
- The concise introduction does not display an economic checklist. Optional help covers camera, construction-card drag and its placement-control alternative, road drawing, building inspection, market diagnosis, and route creation. Construction/category, inspector, Market-tab and trade-map presentation hooks offer the relevant topic. Dismiss hides that topic; Hide tips disables guidance; the pause menu can replay dismissed tips.
- Help preferences persist locally in `Saved/Config/HansaSessionHelp.ini`. They are UI preferences rather than authoritative gameplay state. Loading a game preserves these preferences. Automation uses unique preference files and isolated save-slot directories under `Saved/Automation/Session`; it never overwrites player manual/autosave slots.
- Prosperity paths, selected measures, sustained progress and terminal explanations use the existing authoritative scenario projection. Dismissing a result does not reopen it on subsequent identical projections. Failure explains how to review the city or load an earlier compatible save; success offers review, saving and return to the city.
- First-launch focus is assigned after viewport attachment. Native shared buttons provide hover, pressed, selected, disabled and visible keyboard/controller focus states. Tab/D-pad navigate; Enter/A activates; Escape/B closes the current surface. The session Menu button is available through normal game input. F1/controller View dismisses the current tip. Long progress/results pages scroll with wheel, Page Up/Down or shoulder buttons. Focus returns to the active underlying surface after closing or dismissing.
- Shared scale, high-contrast and large-text preferences are applied throughout; new UI copy uses LOCTEXT. Wrapping text and bounded scrollers allow expansion. No motion is required, so reduced-motion presentation is static. Translated locale packs and human screen-reader certification are not claimed by this implementation.
- Session, help, objective, outcome and navigation semantic nodes are exposed. Session/save modals mask underlying interactive semantics. New session target bounds are recorded in physical pixels with scroll clipping.

## Component inventory and assets

| Component | Native implementation |
| --- | --- |
| Opening/pause shell | Linen dossier, navy header, brass border; 800 logical units wide, 420/480 high, constrained to viewport; enlarged text uses additional height |
| Navigation | Begin/resume/return, close, save/load, review prosperity, help toggle/reset; primary actions pinned outside scrolling content |
| Contextual coach | 400 logical-unit card, native header/body, dismiss and hide controls; six topic states |
| Paths and objectives | Existing scenario family: selected path cards, actual values/status and native progress bars; larger bounded dossier |
| Success/failure feedback | Native explanation and recovery copy, existing outcome family, textual state redundancy |
| Icons and decoration | Shared native state/focus geometry; no painted dynamic text or shipping fullscreen image |

The component inventory was defined before generation in `Docs/Images/UI/SessionP28/ComponentSpecification.md`. Three new **reference-only** images were generated with built-in ImageGen:

| Final reference under `Docs/Images/UI/SessionP28/` | Native dimensions | Final prompt |
| --- | --- | --- |
| `session-p28--composed--paused--1536x1024--v1.png` | 1536 × 1024 | sibling `.prompt.md` |
| `session-p28--pause-menu--focus--1024x1536--v1.png` | 1024 × 1536 | sibling `.prompt.md` |
| `session-p28--context-coach--focus--1536x1024--v1.png` | 1536 × 1024 | sibling `.prompt.md` |

Existing `Docs/Images/UI/Scenario/` objective-row, victory-path-card and outcome-banner references are reused. Final prompt records preserve exact prompts, intended use, native size, generation mode and inspection notes. No new raster is imported into Content: all shipping session surfaces, controls, text and progress bars are native Slate.

Each generated reference was inspected at original resolution. Native captures were compared for hierarchy, palette, restrained border treatment, focus, safe margins and readable controls. The comparison led to reducing the opening/pause shell instead of leaving the oversized progress-page frame around short content. The composed reference's invented logo/tagline and scenery are not shipping requirements. The pause reference's missing navy header is corrected in native code to follow the brief. The coach reference's checkerboard-like backing is not accepted production alpha. No raster is stretched, cropped into production controls or resampled.

## Validation and evidence

- Development build passed: `Saved/BuildArtifacts/20260909-143427624-build-HansaEditor-Win64-Development`.
- All 60 `Hansa.UI` headless tests passed: `Saved/BuildArtifacts/20260909-143829337-automation-Hansa.UI`. This includes loading/begin gating, all six help dismissals and persistence, opt-out/replay, restored pause, and terminal-result acknowledgement behavior.
- All 9 `Hansa.Integration.Save` tests passed: `Saved/BuildArtifacts/20260909-143642814-automation-Hansa.Integration.Save`, including authoritative continuation and scenario sustain/terminal state.
- Twelve final `Hansa.UI.Session.RealViewport` runs passed at 1280×720, 1920×1080, 2560×1440 and 3440×1440, each at 80%, 100% and 140% scale. Each checks initial native focus, Begin/resume clock behavior, pause, prosperity, actual isolated-slot saving, load confirmation, restored pause and construction/roads/market/route help hooks. Buttons are activated through native controller events; context/category changes also use their normal presentation intents.
- **168 original-size PNGs and semantic TSVs** are preserved in `Docs/Images/UI/SessionP28/Native/`. `Scripts/ValidateSessionPresentation.py` verifies dimensions, 36 pinned session targets and 144 coach targets across the matrix. `verification.json` records hashes and explicitly records no resampling.
- Large text, high contrast and reduced motion are enabled for terminal presentation captures. Victory/failure screenshots are deliberately injected presentation fixtures, not claims that the native test economically won/lost the campaign. Inspection coaching is a direct topic fixture; actual terminal persistence is covered separately by the save integration tests. No human first-time playtest was performed.
- Shipping build/exclusion audit passed: `Saved/BuildArtifacts/20260909-143753744-shipping-exclusion-Win64/result.json`. This audits the executable/receipt for excluded editor, test, automation and provider dependencies; it is not a new complete cooked-content package certification. The save codec remains version 3. No gameplay-definition schema, provider or editor module dependency was introduced.

Open [the original-size comparison viewer](../Images/UI/SessionP28/comparison.html) to inspect native captures and the generated references without resizing them. This evidence validates P28's session workflow; it does not certify the entire game's art or all remaining enhanced-MVP work as AAA.
