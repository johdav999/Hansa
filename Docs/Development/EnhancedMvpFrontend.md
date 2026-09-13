# EMVP-P29 — Frontend and system screens

Implemented 2026-09-09. Run `Scripts/LaunchGuiPreview.ps1` to open the Development game at the new title screen. Choose **New game** to begin the enhanced Lübeck slice, or **Continue** to restore the newest compatible manual/autosave. **Settings** is available from the title screen and the in-game **Menu**. Close an already-running preview and launch again to use the rebuilt executable.

## Player behavior

- The native title shell offers Continue, New game, Load game, Settings, Credits and a confirmed Quit. Continue is disabled when no compatible save exists. Development fixtures and automation scenarios are absent from this navigation.
- New game initializes a fresh authoritative runtime from the production definitions, resets simulation and camera position, and opens the concise session introduction. It preserves local help preferences. Loading restores the authoritative save and holds the clock paused until Resume. Loading status paints before the deferred operation; failed operations expose recovery guidance.
- The save manager retains the supported manual/autosave slots, adds an editable manual-save name, and confirms overwrite/load operations. Title-screen load management cannot save an abandoned world. Corrupt and unsupported-format saves remain visible with explanation, unavailable metadata is identified honestly, and Load is disabled. Continue excludes these slots. Failed restore preserves the current authoritative state.
- Autosave runs every five minutes while an active session is playing. Failures pause play and open actionable save feedback. Manual saves remain available after returning to title. Save format remains version 3.
- Settings apply the actual windowed/borderless mode, VSync, master volume, camera pan/rotation/zoom speed, edge scrolling and shared UI scale, high contrast, large text and reduced motion preferences. Display-mode changes require confirmation and revert after 15 seconds or Cancel. System preferences persist in `Saved/Config/HansaSystem.ini`; display settings use Unreal GameUserSettings. No unsupported channel mixer, key remapping or quality presets are advertised.
- Tab/D-pad traverse enabled controls, Enter/A activates, Escape/B returns, and wheel/Page Up/Down/shoulders scroll long settings. Focused controls scroll into view. Closing overlays restores focus to the current underlying surface. Modal semantics mask gameplay controls. Native text uses localization keys and wrapping; shared accessibility styles apply after preference changes.
- Credits identify the engine and project UI fonts and explicitly label the remaining contributor/legal text as a release placeholder.

## Component inventory and generated references

The pre-generation inventory is [ComponentSpecification.md](../Images/UI/FrontendP29/ComponentSpecification.md). Components are the title shell and navigation; session/system navigation; manual/autosave card list and metadata; editable name and action controls; settings rows and shared interface controls; confirmation/loading/error feedback; and credits text. Shared state/focus geometry supplies icons and feedback. No new chart or decorative raster is required.

Five separate built-in ImageGen calls produced the selected references below. Every image is **1536 × 1024 native pixels**, opaque, and **reference-only**. The exact final prompt, generation mode, intended use, dimensions, revision and inspection notes are in each sibling `.prompt.md` file.

| Reference under `Docs/Images/UI/FrontendP29/` | Final prompt record |
| --- | --- |
| `frontend-p29--composed--reference--1536x1024--v1.png` | `frontend-p29--composed--reference--1536x1024--v1.prompt.md` |
| `frontend-p29--navigation--reference--1536x1024--v1.png` | `frontend-p29--navigation--reference--1536x1024--v1.prompt.md` |
| `frontend-p29--save-slot--reference--1536x1024--v1.png` | `frontend-p29--save-slot--reference--1536x1024--v1.prompt.md` |
| `frontend-p29--settings-row--reference--1536x1024--v1.png` | `frontend-p29--settings-row--reference--1536x1024--v1.prompt.md` |
| `frontend-p29--confirmation--reference--1536x1024--v1.png` | `frontend-p29--confirmation--reference--1536x1024--v1.prompt.md` |

The composed image is the family style anchor. A direct reference-file tool call encountered an ACL error; subsequent built-in component generations repeated the exact style tokens and inspected anchor description. No external API workflow was used. All images were inspected at original resolution. All shipping surfaces are reconstructed from native Slate and existing shared fonts/components: there are **no new production raster imports** or full-screen mockup textures.

Reference comparison checked palette, typography hierarchy, focus, margins and control readability. It led to a smaller short-content confirmation shell, navy confirmation header, aligned cancel/destructive actions, tighter title spacing and a shared-style save-name field. The final error-state review also removed misleading empty-slot wording from damaged files. The composed decorative map, uppercase reference wordmark, beveled save-card border and pill toggle are deliberately not copied; native geometry follows the shared brief. References and captures are never resampled.

## Evidence and validation

- Final Development build: `Saved/BuildArtifacts/20260909-153145939-build-HansaEditor-Win64-Development`.
- UI suite: all 61 tests passed in `Saved/BuildArtifacts/20260909-153212164-automation-Hansa.UI`.
- Shipping build and exclusion audit passed: `Saved/BuildArtifacts/20260909-153209558-shipping-exclusion-Win64/result.json`.
- Save integration: all 10 tests passed in `Saved/BuildArtifacts/20260909-152731105-automation-Hansa.Integration.Save`, including fresh-runtime reset, named saving and rejected-corrupt-restore authority preservation.
- Native P28 session regression passed in `Saved/BuildArtifacts/20260909-152745555-gui-repair-1920-1080`. Existing native capture harnesses now enter through the real frontend and wait for attached-widget focus rather than bypassing the player journey.
- Real display-mode cancellation and automatic rollback passed in `Saved/BuildArtifacts/20260909-151213193-gui-repair-1280-720` (`Hansa.UI.Frontend.DisplayRollbackViewport`).
- Native P29 workflow covers 25 states per profile: empty title, settings/audio/controls, credits, New game, playing/pause, named save, overwrite confirmation, return-to-title, valid Continue, corrupt/incompatible saves, accessibility settings, title Load selection/confirmation and paused restore. Controller activation uses actual Slate key events; save naming and accessibility setup use the normal presentation-model intents.
- The final matrix covers 1280×720, 1920×1080, 2560×1440 and 3440×1440 at 80%, 100% and 140% UI scale, with large text/high contrast/reduced motion included late in each flow. `Scripts/ValidateFrontendPresentation.py` validates 300 PNG native dimensions, semantic state and 84 visible primary targets, then preserves original PNG/TSV pairs and hashes under `Docs/Images/UI/FrontendP29/Native/` and `verification.json`.
- Final native matrix completed in the `20260909-153208565` through `20260909-153443285` GUI capture artifact directories. The final validator passed all 300 captures, 12 profiles and 84 primary-target checks.
- Open [comparison.html](../Images/UI/FrontendP29/comparison.html) for the original-size reference/capture viewer. Compact save/confirmation, 1080p settings and ultrawide error states were visually inspected at original resolution; automated bounds/state checks cover all profiles.

## Scope and remaining limits

This completes the bounded P29 frontend implementation, not whole-game release certification. World art remains covered by the subsequent world-assembly work. Credits/legal remains the explicitly requested placeholder. No full clean cook, packaged-release run, all-locale review, human first-time-player study or screen-reader certification is claimed. The five-minute autosave interval has not undergone a long-duration soak; the save path itself has automated integration coverage. Native save-name typing is wired through the edit control, while automation sets the name through its presentation model.

No new gameplay definition schema, editor data model or provider integration was introduced. Runtime modules retain one-way separation from editor/provider code. Shipping verification below scans the built executable and receipt for excluded editor, automation, test, worker and provider tokens; it is not a replacement for the later full packaged-content audit.
