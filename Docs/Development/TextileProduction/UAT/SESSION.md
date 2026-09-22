Promotion update: user approved and promotion completed; see [PROMOTION.md](../PROMOTION.md). This session records the original candidate review.

# Textile production playable UAT — 2026-09-19

Revision: isolated TextileProductionV2 candidate, 136 definitions, freshly saved/reloaded registry hash `386F8F6145FE5135`. Original V1 snapshot and accepted catalog pins remain unchanged. All generated media remains staged for explicit promotion approval.

Launch: `Scripts/PreviewTextileProduction.ps1`; automated review: add `-Verify`. The launcher opens the surveyed Lübeck staging map in Development with the explicit candidate flag and enforced registry hash.

## Results and evidence boundaries

| Flow | Result | Evidence |
|---|---|---|
| Browse Craftsmen chains and place four workshops | Pass: native chain/card actions, placement presenter, and actual imported mesh bindings | Four rendered runs below; tray/card-focus/workshop PNG and semantic TSV captures |
| Weaver supplies Tailor physically | Pass in deterministic integration test: cloth observed in transit, Tailor output exact; disconnected road prevents delivery | `WeaverTailorPhysicalDelivery` |
| Exact recipes and blockers | Pass in mechanics tests: specified input/output, missing inputs, workforce, storage, market sourcing and ownership | `Hansa.TextileProduction` suite |
| Ropewalk modes and save/load | Pass: independent modes; selected and pending recipes survive save/load without duplicate consumption; native recipe buttons retain identity and focus | Mechanics suite plus rendered recipe action/save round trip |
| Craftsmen needs | Pass: four residents consume 0.004 clothing and 0.004 candles per tick from market stock | `CraftsmenConsumeClothingAndCandles` |
| Viewport, scale and large text | Four rendered runs pass; compact card and Ropewalk inspector visually inspected at original pixel dimensions | Matrix below |
| Imported 3D family | Existing Blender reimport and Unreal bounds/material/LOD/collision evidence inspected; footprint binding rejects oversized geometry | `SourceArt/Generated/Buildings/TextileProductionV1/evidence/` |

The rendered review uses normal native actions and placement logic but starts with an empty city without established workforce. It does **not** prove a staffed production economy by screenshots. Physical logistics, batch production and household consumption are proven by deterministic integration tests. Hardware-controller input and a full manual economic playthrough remain separate acceptance checks.

## Final rendered matrix

All runs completed `Hansa.UI.TextileProduction.PlayableReview` successfully with exit code 0, after the final card focus and compact layout changes.

| Viewport | UI scale / text | BuildArtifacts log directory |
|---|---|---|
| 1280×720 | 140%, large text | `20260919-175714280-textile-playable-review` |
| 1920×1080 | 100%, normal | `20260919-175754696-textile-playable-review` |
| 2560×1440 | 80%, normal | `20260919-175835579-textile-playable-review` |
| 3440×1440 | 100%, normal | `20260919-175918022-textile-playable-review` |

Logs live in `Saved/BuildArtifacts/<directory>/Unreal.log`. Durable PNG/TSV captures are in `screenshots/`, with viewport and scale in each filename. At the compact setting, the construction tray and inspector require native scrolling; focus remains visible and recipe actions remain readable. Semantic focus is checked by automation, not by a physical controller.

Five mechanics tests passed in `Saved/BuildArtifacts/20260919-175341879-automation-Hansa.TextileProduction/UnrealEditor.log`. Fresh staging/reload logs are `Saved/Logs/TextileStageV2.log` and `TextileReloadV2.log`. Definition, ratio, UI prompt and model details are in `../README.md`.

## Issue ledger

| ID | Severity | Issue | Resolution / remaining action |
|---|---|---|---|
| UAT-TEX-001 | P1 | Original module DLL lock prevented linking | Resolved: editor was closed; editor and runtime builds linked and review ran |
| UAT-TEX-002 | P1 | Candidate enumerated newer Core IDs absent from the old staging snapshot | Resolved: discover the isolated candidate root and enforce its exact count/hash |
| UAT-TEX-003 | P1 | Tailor/Chandler geometry exceeded footprint width | Resolved: 3×2 cells; binding rejects meshes outside footprint |
| UAT-TEX-004 | P2 | Rope recipe names exposed stable-ID spelling and refresh replaced focused widgets | Resolved: authored localized labels and retained native widget identity |
| UAT-TEX-005 | P2 | Compact construction content escaped its frame | Resolved: native outer scrolling and focus reveal; inspected 720p/140%/large-text capture |
| UAT-TEX-006 | P2 | Capture test used stale presentation data after a command refresh | Resolved: inspect the snapshot before mutation; final four runs pass |
| UAT-TEX-007 | P2 | One earlier rendered run exited unexpectedly (-1073741819) | Retained log `20260919-175407829-textile-playable-review`; no diagnostic cause established. Final four-run sweep passed; monitor in broader soak testing |
| UAT-TEX-008 | P2 | Existing M_Road_Earth material lacks SplineMeshes usage and falls back | Outside textile changes; warning reproduced in final review and remains a visual release issue |

## Approval and release gates

- Explicit approval is required before promoting staged definitions, models or generated imagery into the accepted production catalog.
- Complete hardware-controller and manual staffed-economy playthrough checks.
- Run a clean cooked-package media/reference audit. The Shipping binary/receipt exclusion audit does not establish cooked asset exclusion.
- Resolve the existing road material warning and monitor the isolated unexpected rendered-run exit during broader soak testing.
## Final build verification

- Development editor: `Saved/BuildArtifacts/20260919-175643971-build-HansaEditor-Win64-Development` passed.
- Final Development runtime: `Saved/BuildArtifacts/20260919-180201117-build-Hansa-Win64-Development` passed; copied summary `runtime-build.json`.
- Final Shipping build and binary/receipt exclusion audit: `Saved/BuildArtifacts/20260919-180214048-shipping-exclusion-Win64` passed; copied summary `shipping-exclusion.json`.
- Scoped `git diff --check` passed; only Git line-ending conversion notices were emitted.
