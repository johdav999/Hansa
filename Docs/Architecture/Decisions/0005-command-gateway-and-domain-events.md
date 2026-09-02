# ADR-0005 — Typed command gateway and ordered domain events

- Status: Accepted
- Date: 2026-09-02
- Decision owners: Hansa project

## Context

Player input, server RPC handling, AI and controlled automation must request the same authoritative mutations. If any caller can write simulation records directly, duplicate validation, bypass authority checks or publish events before commit, single-player, multiplayer and automated evidence can disagree or expose partial state.

## Decision

- After validated initialization/restore, `FHansaGameplayCommandGateway::ExecuteTick` is the sole public authoritative tick-mutation entry point. The fixed-step executor remains private to that gateway.
- Every command is a closed typed payload plus a versioned header containing a stable command ID, issuing-house authority context, origin, principal ID, requested tick and global sequence.
- Command IDs and sequences are independently monotonic. IDs provide stable request correlation and uniqueness; sequences determine application order.
- Caller-supplied fingerprints are forbidden. The runtime hashes the complete versioned header and active typed payload.
- All commands for one tick are validated and applied to a working copy in array order. Any header, authority, ordering, capacity or payload failure rejects the entire batch, preserves the original state and transient cache, advances no time and publishes no events.
- Player, AI, multiplayer RPC and controlled automation origins receive no differentiated mutation privilege. Target ownership is validated against authoritative state. Later network/session work must authenticate the principal-to-house claim before submission without adding a second mutation path.
- Successful changes publish immutable events in command order. Each event records a globally monotonic sequence, simulation tick, source command ID and issuing house. Only the authoritative published-event count is retained in simulation state; event payload batches are not an unbounded permanent history.
- The initial create/cancel/no-op payloads and lifecycle record are representative kernel contracts, not city gameplay. Feature prompts replace or extend the closed command/event families through the same gateway.

## Consequences

Positive:

- Every future caller shares validation, ordering, rollback and evidence correlation.
- Late validation failures cannot leak partial records or presentation events.
- Equal initial state and typed command stream reproduce equal state fingerprints and event order.

Costs:

- Tick application copies authoritative state until later profiling justifies a more specialized transaction journal.
- Command and event schema evolution must be explicit and replay-aware.
- Command IDs must be assigned monotonically by the authority before gateway submission.

## Compliance

`HansaSimulation` tests must cover create/cancel/no-op application, structured stable causes, unknown or mismatched authority, command ID and sequence ordering, late-batch rollback, replay equality, payload divergence and event ordering across ticks. Runtime code must retain no Actor, UObject, World, UI, editor, automation-transport or provider dependency.

This infrastructure adds no designer-authored definition or reflected gameplay property, so the editor schema/migration matrix has no applicable field in `S01-P03`. Future feature commands that consume new definition fields remain subject to the full editor/game parity contract.

## Deferred

- Wire serialization and protocol envelopes for RPC, save replay and automation transport.
- Authenticated session principal-to-house mapping and idempotent network retry windows.
- Retention policies for audit/timeline events and durable event serialization.
- Replacing whole-state transaction copies if profiling demonstrates a material budget problem.
