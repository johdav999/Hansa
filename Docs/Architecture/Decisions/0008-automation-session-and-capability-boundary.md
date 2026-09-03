# ADR-0008 — Automation sessions are opt-in, authenticated and capability bounded

- Status: Accepted
- Date: 2026-09-02
- Decision owners: Hansa project

## Context

The external MCP sidecar and future test clients need a stable boundary into development builds, but an automation surface can easily become a second gameplay API or an accidental remote console. The boundary must be useful for typed inspection and controlled actions while remaining disabled by default, incapable of client-driven privilege escalation, and absent from Shipping.

## Decision

- `HansaAutomation` remains a separate `DeveloperTool` module with one-way dependencies on runtime modules. Runtime modules never depend on it.
- S02-P02 establishes an in-process, transport-neutral `FHansaAutomationSessionService`; it opens no pipe, socket or listener and performs no per-frame work.
- Protocol version starts at `1.0`. A client major must match and its minor must not exceed the process minor. Incompatibility fails before session creation.
- Process startup owns the security policy. Automation requires explicit non-Shipping enablement plus a 16–128 character short-lived token delivered through the process-scoped `HANSA_AUTOMATION_TOKEN` environment variable so Unreal command-line logging cannot disclose it. The configured permission ceiling defaults to `ReadOnly`; client messages cannot raise it.
- Exactly one controller may own one active session. Session IDs are process-generated and controller/session identity is checked on every session-bound operation.
- Permission levels are ordered `ReadOnly`, `ControlledActions`, then `FixtureControl`. Capabilities are independently discovered, explicitly requested and granted; permission alone never implies a capability.
- Every operation is an enum-backed allowlisted protocol operation mapped to one capability and minimum permission. Unknown values fail with `OperationUnsupported`.
- Request contexts carry bounded correlation IDs, endpoint-captured monotonic enqueue time and positive timeouts capped at 30 seconds. Expired work fails before authorization.
- All failures use stable machine error codes plus correlation ID, safe message, remedy and retryability. Authentication values are never returned or logged.
- The public boundary contains no UObject/Blueprint reflection, console command, filesystem path, raw memory/state pointer, generic query language or client-selected class/asset loading surface.
- Shipping continues to omit the module at target composition, reject its compilation, compile runtime seams to zero and scan representative artifacts for automation markers.

## Consequences

Positive:

- The S02-P03 sidecar can adapt transport messages to one reviewed contract without inventing authorization rules.
- Read-only and mutating access remain independently negotiable and auditable.
- Disabled Development play exposes no session capability and adds no endpoint or tick work.

Costs:

- Every later query/action family needs an explicit capability and operation mapping.
- One-controller ownership intentionally rejects concurrent automation clients until a reviewed use case changes the policy.
- Short-lived token delivery and transport framing require careful process-launch integration in the sidecar.

## Compliance

Automation tests must prove default disablement, compatible open/get/close, single-controller ownership, protocol and authentication rejection, missing capability, permission ceiling, rejected/unknown commands, timeout/correlation behavior and Shipping unavailability. Development builds, full regression and Shipping exclusion remain mandatory gates.

## Deferred

- Named-pipe framing, sidecar reconnect and token handoff in S02-P03.
- Semantic UI, screenshots, fixtures, gameplay queries and wait/assert implementations in later S02 prompts.
- Authenticated loopback transport or multi-controller sessions; neither is enabled by this decision.
