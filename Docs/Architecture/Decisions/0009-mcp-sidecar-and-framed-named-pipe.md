# ADR-0009: MCP sidecar and framed named-pipe transport

- Status: Accepted
- Date: 2026-09-02
- Owners: Automation and test infrastructure
- Depends on: ADR-0001, ADR-0004, ADR-0008

## Context

Codex needs a standard local tool boundary, while the running development game needs a narrow transport that preserves the authenticated session policy from ADR-0008. Embedding MCP, Node.js, provider SDKs or credentials in Unreal would reverse the intended dependency direction and risk Shipping leakage. An Unreal crash must not destroy the orchestrator process or its already collected evidence.

## Decision

`Tools/HansaMcp` is a private, zero-dependency Node.js 22 sidecar. Codex launches it as an MCP STDIO server. STDIN and STDOUT contain newline-delimited UTF-8 JSON-RPC messages only; operational logs use STDERR. The S02-P03 MCP surface is exactly `capabilities_get`, `session_start`, `session_get`, `session_stop`, `ping` and `health`.

On Windows, the sidecar connects to one explicitly named local pipe. Unreal creates that pipe only when the non-Shipping process has `-HansaAutomation`, a valid `HANSA_AUTOMATION_TOKEN`, and a safe `HANSA_AUTOMATION_PIPE`. The pipe rejects remote clients and accepts one controller. Frames use a four-byte unsigned little-endian payload length followed by one UTF-8 JSON object, with a 64 KiB maximum. Wire envelopes have `schemaVersion: 1`, a bounded request ID, operation, controller/session identity, timeout and payload.

Unreal remains authoritative for protocol compatibility, capabilities, authentication, permission and session state. The sidecar adapts those structured results to MCP and does not invent gameplay truth. Transport connection uses bounded exponential backoff. A dropped in-flight request is never replayed automatically because its outcome may be ambiguous, especially for session mutations.

The token is read from the inherited process environment and is sent only in `session_start`. Neither process logs it. Sidecar logs redact secret-named fields and known secret values. A codec-backed fake in-process endpoint supplies deterministic contract tests without Unreal or a live game.

## Consequences

Positive:

- Codex can use the same local MCP server from the desktop app, CLI or IDE configuration.
- Unreal has no MCP library, npm dependency, provider credential or external-tool dependency.
- Framing, response correlation, reconnect and error forwarding are testable in normal CI without launching Unreal.
- Disabled Development and all Shipping builds still open no endpoint and perform no endpoint polling.

Costs and limitations:

- Node.js 22 or newer is required on development/CI hosts that run sidecar tests.
- S02-P03 implements Windows named pipes only; an authenticated opt-in loopback transport remains unnecessary.
- If the sidecar process dies after opening a session, the game retains that single-controller session until the game stops or a future lease/recovery contract is added.
- The ticker-polled endpoint is intentionally limited to small control messages; later large evidence payloads require bounded paging or out-of-band artifact references.

## Compliance evidence

- `Scripts/RunHansaMcpTests.ps1` runs framing, fake-endpoint, MCP lifecycle, structured-error, redaction and delayed-endpoint reconnect tests.
- `Scripts/VerifyRepositoryConventions.ps1` requires the sidecar package/schema/test entry point, rejects npm dependency sections for this scaffold and checks sidecar source for Unreal header dependencies.
- Unreal automation tests continue to prove fail-closed startup, session policy and Shipping source exclusion.
- `Scripts/VerifyShippingExclusion.ps1` continues to scan the Shipping target receipt/executable for `HansaMcp` and automation markers.

## Deferred

- Semantic UI, observable waits and screenshot/evidence operations begin in S02-P04.
- A session lease or authenticated recovery operation is deferred until a real need is proven.
- Loopback WebSocket support and multiple controllers remain out of scope.
