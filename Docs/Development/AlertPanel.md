# Alert panel implementation — 2026-09-16

## Result
Native Slate navy/brass shell, linen category cards, severity artwork and label,
affected object, visible age, problem/cause, and Details/Locate/Snooze/Pin actions.
The action grid uses two columns and shared focus/hover/pressed/selected styles.
The panel is 320 or 336 Slate units wide and its scroll area is bounded by the
available viewport height. Collapsing hides the complete body. Up to three category
representatives expand; remaining alerts have individually reachable Details rows.
Existing approved ImageGen status artwork is reused through SHansaGlyph.

## Flicker correction
Previously RebuildAlerts compared all alert fields and cleared/recreated the list
when age or economic evidence changed. It now distinguishes structure from content.
Text, age, causal tooltips, severity icon/accent, pinned tracker text and snoozed
labels update existing widgets. Structural changes preserve scroll offset and
restore focus where possible. No animation or polling was added.

## Files
- Source/Hansa/Private/UI/SHansaRootHud.cpp
- Source/Hansa/Public/UI/SHansaRootHud.h
- Source/HansaTests/Private/UI/HansaHudPolishTests.cpp
- Source/HansaTests/Private/UI/HansaAlertPanelCaptureTests.cpp
- Scripts/VerifyAlertPanel.ps1

## ImageGen deliverables
All three are visual references, not imported production bitmaps. Generated with
built-in ImageGen; original dimensions retained; no resizing. Native implementation
uses shared project styles, fonts and existing generated status icons.

| Component | Native dimensions | Repository reference and sibling prompt |
| --- | --- | --- |
| Composed panel | 1024×1536 | Docs/Images/UI/Alerts/alerts--screen--default--1024x1536--v1.png |
| Card | 1254×1254 | Docs/Images/UI/Alerts/alerts--card--default--1254x1254--v1.png |
| Shell | 1254×1254 | Docs/Images/UI/Alerts/alerts--shell--default--1254x1254--v1.png |

Each file has a same-basename .prompt.md containing the exact final prompt, target
and generated dimensions, mode and inspection notes. Component inventory and states
are in Docs/Images/UI/Alerts/README.md. Existing shared button/icon references are
reused. No new raster production asset was needed or imported.

The original references were inspected for hierarchy, palette, safe margins and
text. The composed reference's four-across actions are superseded by the card's
two-column action layout at the real display width. Generated decorative details
and static example text are not copied into shipping rasters.

## Validation
Run Scripts/VerifyAlertPanel.ps1. Evidence is saved in Saved/AlertPanel.
The native regression exercises 120 age/evidence updates plus severity/text changes
and verifies button identity. Existing HUD/inspector tests cover alert action routing,
pinning, snoozing, restoration, semantic focus and localization contracts.

Viewport tests use the actual game HUD and an explicitly authored eight-alert
presentation fixture after starting the scenario. Stages: normal, 120 updates,
collapsed, high contrast/large text/reduced motion, 80% scale, and 140% scale with
controller focus revealing the last market alert. Captures are unscaled PNGs with
semantic bounds beside them. These fixtures do not modify gameplay or saves.

Final run: HansaEditor Win64 Development build succeeded. All 13 HUD/inspector regression tests passed. RealViewport passed at 1280x720, 1920x1080, 2560x1440 and 3440x1440, each capturing six states (24 screenshots). Native screenshots were visually inspected at actual pixel size: normal, high contrast/large text and 140% focus-revealed overflow. Selected full-resolution screenshots are preserved in Docs/Development/AlertPanel. Text remained readable, panel bounds stayed within the viewport, and offscreen actions were reachable. No Shipping package validation was performed.

## Limits
This verifies native game/editor development builds, not a packaged Shipping build.
Normal content updates retain widget identity; adding/removing/reordering alerts or
pinning/snoozing intentionally changes structure and restores available focus.

