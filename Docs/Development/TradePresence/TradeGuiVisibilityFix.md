# Trade GUI visibility and first-station lock repair — 2026-09-22

## Root causes and changes

- The route inspector appended presence, upgrades, specializations, orders and route activation to one long scroll area. The reported 2730x888 game capture shows these below the visible area. Four persistent native section shortcuts now reveal Route actions, Foreign presence, Specializations and Station orders. Existing shared action styles, fonts and surfaces are reused; no new artwork or theme.
- Office upgrade was missing from controller order. Presence controls were missing from focus-scroll prefix handling. Focus now reveals controls by scroll ancestry, upgrade joins the focus order when available, and traversal skips disabled controls. Semantic availability no longer overwrites a disabled specialization with an enabled state.
- Station orders disappeared completely before establishment. The section is now discoverable with an explicit prerequisite explanation, while unavailable order commands remain hidden.
- Establishment eligibility incorrectly used upgrade funding availability, which reads materials from the not-yet-created station inventory. Proposal now checks contribution eligibility; the separate funding command still validates and consumes actual cargo and money.
- The authoritative proposal required investment already spent before permitting the first investment. It now counts the planned authored construction investment, as the requirements projection already does, and enforces the other displayed contribution requirements. Proposal creates empty storage; it does not grant free construction. Office upgrade is not offered before an active station exists.

## Verification

- Editor Development compilation succeeded.
- `Hansa.UI.TradeMap`: 4 tests pass, including native section-navigation regression at 1280x720, 1920x1080 and 2730x888; keyboard specialization focus reveal; locked orders; persistent shortcuts; route-draft guard.
  - `Saved/BuildArtifacts/20260922-163126726-automation-Hansa.UI.TradeMap/`
- `Hansa.Integration.TradePresence`: 8 tests pass. Establishment now starts with zero prior investment and no station inventory, verifies UI proposal eligibility, then exercises ordinary proposal/funding/completion/save/reload/closure. Office progression also checks enabled upgrade focus.
  - `Saved/BuildArtifacts/20260922-163055969-automation-Hansa.Integration.TradePresence/`
- Game Win64 Development build succeeded.
  - `Saved/BuildArtifacts/20260922-163155608-build-Hansa-Win64-Development/`
- Native screenshot bundles: `Saved/TestEvidence/Automation/TradeSectionNavigation/`. Visually inspected the 1280x720 presence view and 1920x1080 specialization view. These are actual isolated Slate renders, not assembled-world or packaged-game evidence. The screenshot service supports only 720p/1080p; ultrawide receives layout/navigation/visibility assertions, not a saved capture.

## Limits and use

Restart the editor/game to load rebuilt native modules, then open Baltic trade and use the four shortcuts above the right-hand scroll panel. Existing gameplay contribution requirements still apply. A previously packaged executable is not updated by a source/Development build and must be repackaged separately.

No catalog assets, save schema, or editor definition schema changed. Existing unrelated working-tree edits were preserved. This scoped repair does not certify the broader TR-14 release gate or add missing later-stage political action screens; privilege/project/governance text in this inspector remains a review summary.
