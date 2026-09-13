# EMVP-P23 — Native HUD and contextual inspector

## Component inventory and states

This work assembles the approved P21 components and preserves P22 construction.
The P21 composed anchor, TopBar, Panel, Button, Tooltip, Notification, Focus and
Icon references resolve the visual language. No new raster component or unresolved
artwork is required, so P23's instruction to generate only unresolved visuals
requires no additional ImageGen pass. All shipping output is native Slate.

| Component | Native implementation | States |
| --- | --- | --- |
| Screen shell and safe areas | Existing HUD overlay, bounded edge hosts | default, inspector open, construction open |
| Treasury, population, workforce, date | Shared type and surface tokens; current model values | current, unavailable |
| City breadcrumb and navigation | Shared actions and tooltips | default, hover, pressed, selected, focus |
| Pause and speed | Labeled native shared actions | default, active speed, focus |
| Alerts and pinned tracking | Bounded grouped native rows and shared actions | notice, warning, critical, snoozed, pinned, empty |
| Selection and notification layers | Native labels and shared feedback surfaces | selection, empty, notice, error |
| Context inspector | Shared panel hierarchy, native scrolling | identity/state, result, flows/needs, cause/remedy, actions, recent history |
| Inspector controls | Shared actions and focus navigation | enabled, disabled with reason, selected, confirmation, error |
| Loading and empty content | Shared state surfaces and native text | loading, unavailable, no selection, no flows/history |
| Accessibility | P21 fonts, contrast, focus rings and immediate motion policy | high contrast, large text, reduced motion, long labels |
| Charts, overlays, imagery | Existing world selection and P04 footprint; no new chart or raster | existing world states |

Palette, spacing, type, contrast and motion follow `Docs/UIDesignBrief.md`.
Text and numbers remain live, localized widget content. No gameplay identity,
simulation rule, economic definition or save schema is changed.

## Implementation and acceptance

Implemented 2026-09-09. The two-row status bar keeps resource values and the city
breadcrumb visible at both reference resolutions. Shared native actions show the
active Pause/1×/4×/12× speed and provide tooltips. The existing P22 construction tray
and world selection projection remain connected to production intents.

Alert buttons retain their widget identity across unrelated resource updates.
Explain remains in each group header while details scroll. Snoozed alerts have a
visible Restore action; hidden actions reject focus and activation. Notifications
occupy the lower left, away from the inspector.

The inspector uses live building/residence projections and the shared causal model.
Its order is identity/state, result, flows or needs, cause/remedy, actions, and history.
Building history now comes from actual simulation events, with an explicit empty
fallback. Actions retain native widget identity across state changes, show disabled
reasons, and reveal themselves through scrolling when focused. Loading, error and
empty selection states hide gameplay actions and retain Close and recovery guidance.
These are presentation states; gameplay definitions, migrations and saves are unchanged.

Native action focus updates semantic focus, including focus acquired directly by Slate.
Tab and D-pad navigation follow visible enabled controls; Accept invokes the actual
focused action. P21 high contrast, large text and reduced-motion preferences flow
through the HUD and inspector. Large text receives a taller status bar; long city
labels ellipsize with a complete tooltip. No animated transition is required.

## Verification

UE 5.8 Win64 DebugGame build passed. Focused automation passed:

| Suite | Tests | Local evidence under Saved/BuildArtifacts |
| --- | ---: | --- |
| Hansa.UI.HUD | 5 | 20260909-070449433-automation-Hansa.UI.HUD |
| Hansa.UI.Inspector | 6 | 20260909-071706154-automation-Hansa.UI.Inspector |
| Hansa.UI.Style | 5 | 20260909-070219036-automation-Hansa.UI.Style |
| Hansa.UI.BuildMenu, with rendering | 10 | 20260909-070732790-automation-Hansa.UI.BuildMenu |

Final native runs passed: `20260909-071747786-p23-native-1280-720` and
`20260909-071816240-p23-native-1920-1080` under `Saved/BuildArtifacts`.

`Hansa.UI.HudPolish.RealViewport` captures thirteen states at each native resolution:
default, speed, production, cause focus, action focus, history, residence, alert,
loading, error, empty, accessibility, and long localization. It verifies the live
selection callback, keyboard speed activation, semantic synchronization from native
focus, controller navigation/Accept, and pointer routing to the native Pause action.
The offscreen window requires an explicit window-to-button path and synthetic hover;
this covers Slate pointer handling and replies, not OS hit testing or physical hardware.
World selection uses the production selection callback on real map actors, not a
physical world-click trace. Warning/notification and unavailable-state captures use
explicit presentation fixtures; the production and residence captures use real data.

Run after a DebugGame build:

```powershell
Scripts/CaptureHudInspector.ps1 -Width 1280 -Height 720
Scripts/CaptureHudInspector.ps1 -Width 1920 -Height 1080
python Scripts/ValidateHudInspector.py
```

The validator checks all 26 PNG dimensions, paired semantic layouts, edge-panel and
speed-control bounds, unavailable-state action visibility, and production sections.
The default HUD leaves at least **80.1% at 1280×720** and **78.8% at 1920×1080** of
the viewport unobstructed by its top bar, alert stack and construction tray.

## Assets and inspection

`Docs/Images/UI/HudInspector/Native/` contains the 26 native PNGs, paired TSV semantic
snapshots and a SHA-256 manifest. Each PNG is exactly 1280×720 or 1920×1080. Capture
mode is Unreal Slate viewport readback with rendering enabled; no raster resampling.
These are acceptance evidence, not shipping textures or generated GUI mockups.

Shipping output consists of native Slate widgets in `Source/Hansa/Private/UI/` and
shared P21 fonts/style assets already in `Content/Hansa/UI/`. P21 approved component
references are reused. New generated masters, imports and prompt sets: none.

Original-resolution inspection covers hierarchy, readable controls, focus outlines,
notification placement, scrollable inspector sections, unavailable-state spacing,
large text and long labels. Inspector actions/history may require scrolling at 720p;
focus reveals the target immediately. No physical-controller session, Shipping cook,
or full translation review was performed for this presentation-only change. Existing
P22 rendered regression coverage passed; broader P24–P29 screen acceptance remains
separate.
