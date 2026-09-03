# ADR-0010: Semantic UI, observable waits and native evidence

- Status: Accepted
- Date: 2026-09-02
- Owners: Automation and UI infrastructure
- Depends on: ADR-0004, ADR-0008, ADR-0009

## Context

Automation must inspect and operate Hansa UI without binding external clients to volatile Slate/UMG class names, widget paths, English display text or pixel coordinates. Synchronization based on arbitrary delays is unreliable, and screenshots resized after capture can hide layout defects. Evidence also needs enough state context to diagnose a failure while remaining development-only.

## Decision

`HansaAutomation` owns a widget-class-neutral semantic registry. Every exposed node has a stable namespaced ID, role, label, boolean state, integer pixel bounds, enabled/visible/focus properties, parent/child relationships and an explicit action allowlist. Native widgets register descriptors and bounded action handlers; the registry does not discover or serialize widget class names.

UI actions require `ControlledActions`; inspection remains `ReadOnly`. S02-P04 exposes only `semantic_find`, `semantic_state`, `semantic_activate`, `semantic_focus`, `wait_for` and `screenshot_capture` in addition to the existing session surface. Unknown IDs, unadvertised actions and missing capabilities fail structurally.

`wait_for` observes one declared semantic boolean predicate (`exists`, `visible`, `enabled`, `focused`, `selected`, `loading`, `warning` or `error`) on the automation ticker. A pending request completes when the predicate matches or its monotonic deadline expires. It never sleeps and never blocks the game thread.

Native screenshots support exactly 1280×720 and 1920×1080. The Slate surface is arranged at the requested client size and captured into that exact pixel buffer. Those pixels are encoded directly to PNG; there is no post-capture resize. Every capture writes an ignored bundle beneath `Saved/TestEvidence/Automation/S02P04/<bundle-id>/` containing the PNG, capture metadata and the synchronized semantic snapshot. The bundle ID is bounded and cannot select an arbitrary path.

The first integration surface is an automation-only native Slate proof screen. It exists only in the non-Shipping `HansaAutomation` module after explicit transport enablement. It proves stable find/state, activation, focus, warning state and exact-size capture without adding a gameplay shortcut or production UI asset.

## Consequences

Positive:

- UI automation survives native widget-class and layout refactors when the semantic contract is unchanged.
- Assertions synchronize on observable state and produce bounded structured timeouts.
- Captures expose genuine target-resolution layout behavior and are correlated with semantic state.
- Production raster assets are unnecessary; styling, text, state and focus remain native and accessible.

Costs and limitations:

- Native screens must deliberately register and synchronize semantic nodes; unregistered widgets are invisible to automation.
- The first registry is process-local and the proof surface is intentionally small. Gameplay screens add their own stable nodes in later prompts.
- One framed request remains in flight per sidecar, so a pending wait serializes other calls on that connection.
- `-NullRHI` proves exact dimensions and evidence mechanics but produces a null-renderer buffer; visual acceptance uses a rendered Development process.
- UE's native Slate screenshot API requires a real platform window; `-RenderOffscreen` fails structurally with `CaptureUnavailable` instead of substituting synthetic pixels.

## Compliance evidence

- Unreal tests cover stable descriptors/relationships/actions, monotonic revisions, event-loop predicate completion/timeouts, both exact PNG dimensions, metadata and unsupported-size rejection.
- Node contract tests cover the MCP schemas, permission boundary, semantic state transitions, waits, both capture sizes and structured errors.
- `HansaAutomation` remains forbidden in Shipping and `Saved/TestEvidence` remains ignored.
- Reference images and prompt records live under `Docs/Images/UI/AutomationProof`; the interactive proof screen is native Slate.

## Deferred

- Gameplay-screen semantic coverage, fixture state, structural gameplay queries and combined golden evidence arrive with their owning feature prompts.
- Screenshot comparison thresholds and platform reference baselines are deferred until a production screen exists.
