# GUI production-readiness root-cause review

Review basis: user-supplied running-game screenshot, 2026-09-09, and current source.
This is a diagnosis; no GUI fixes were made in this review.
The exact binary revision and OS/UI scale of the supplied screenshot are unknown.

## Profile and evidence

Product: Hansa, native Unreal Slate game HUD. Role: local player.
Flows: inspect an operating bakery; read objectives/alerts; use inspector actions.
Scene: user-reported Lübeck session. Launch procedure: DebugGame executable with
Hansa.uproject and /Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP, -game.
Evidence: user attachment codex-clipboard-38f4ae90-59f9-4452-aaea-9b3aefe2f73f.png.
Expected: compact, legible, contextual HUD following UIDesignBrief.
Observed: large repetitive controls, diagnostic prose dominating an operating
building inspector, competing panel emphasis, and substantial scrolling.

## Open issue ledger

| ID | Severity | Flow / owner | Finding and source | Acceptance / regression |
| --- | --- | --- | --- | --- |
| GUI-RC-01 | P2 | All screens / HansaUiStyle | Brief specifies Body 15–17 px; GetTypography passes Size=16 directly to FSlateFontInfo, whose size is points converted at 96 DPI. This implies about 21.3 Slate units before further scaling. | Define token units explicitly; measure rendered typography at reference scale and validate against the brief. |
| GUI-RC-02 | P2 | Inspector actions / HansaUiComponents and SHansaContextInspector | Noncompact SHansaAction combines a 48-unit minimum content height, 20 units vertical normal padding, and a reserved caption line. Inspector creates every action with this default. | Dedicated compact secondary/action-row treatment, appropriate primary emphasis, stable hit targets, no unnecessary state caption. Inspect a realistic full inspector. |
| GUI-RC-03 | P2 | Healthy building / inspector presentation model | Expanded cause/evidence/remedy prose is shown for an operating building; implementation terminology such as authoritative projection reaches player-facing copy. | Healthy default prioritizes output, inputs and useful actions; explanations expand on demand; diagnostic provenance stays out of normal player copy. |
| GUI-RC-04 | P2 | Integrated HUD / screen composition | Almost every action is a large filled rectangle; alert, inspector and toolbar compete rather than using a coherent visual hierarchy. | Reconstruct an approved integrated HUD composition and review primary/secondary/tertiary emphasis in actual play. |
| GUI-RC-05 | P2 | Release acceptance / validation | ValidateHudInspector checks image dimensions, outer bounds, semantic presence and default occupancy. It does not reject excessive inner control size, weak hierarchy or repetitive prose. | Separate functional, visual-conformance and real-player usability acceptance; test the ordinary full HUD state and supported scales, not only isolated semantic paths. |

## Root cause

Production readiness was inferred from functional coverage, shared palette usage,
native widgets and successful capture/semantic checks. These are useful engineering
checks but insufficient evidence of finished product design. The visual review
failed to reject clear design and density defects. Earlier completion reports
overstated readiness: functionality was implemented, while production visual
acceptance remains open.

Corrective sequence: repair shared token units and component density, recompose
one integrated HUD/inspector flow against the brief, simplify default information,
then validate the real screen before propagating the system to other screens.
Unfinished terrain/world presentation visible in the attachment is a separate
contributor to the overall impression and is outside this GUI diagnosis.

## Repair follow-up — 2026-09-09

The diagnosis above is retained as the historical review. The cross-screen fixes, native capture matrix, ImageGen prompt records and remaining acceptance gates are documented in [GuiRepairSession.md](GuiRepairSession.md). [Compare generated references with the native game](GuiRepairComparison.html). Engineering test passes do not close artistic or player-usability acceptance.
