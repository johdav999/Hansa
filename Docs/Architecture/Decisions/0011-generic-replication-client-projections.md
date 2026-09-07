# ADR-0011 - Generic replication with owner-scoped client projections

- Status: Accepted
- Date: 2026-09-06
- Decision owners: Hansa project

## Context

The deterministic simulation and command gateway already separate authoritative state from read-only projections, but they did not authenticate a network principal's house claim or define what each client may observe. Replicating `FHansaSimulationState` would expose mutable/private state, make relevancy expensive, and couple network compatibility to the save/domain layout.

The MVP needs a two-client authority proof now. Production-scale actor counts, platform sessions, prediction, latency compensation, and measured Replication Graph or Iris requirements do not exist yet.

## Decision

- The server or standalone authority exclusively owns `UHansaRuntimeSimulationHost`. Clients submit bounded intents by reliable PlayerController server RPC.
- `FHansaMultiplayerAuthority` maps an authenticated connection principal to one existing house before constructing the normal `MultiplayerRpc` gameplay command. Clients cannot supply house ownership, authoritative tick, global order, generated IDs, prices, costs, elapsed time, or resolved output.
- Per-client sequence and nonce validation rejects duplicates and gaps. The server generates globally ordered command identity. Gameplay ownership and rule validation remain in `FHansaGameplayCommandGateway`.
- Unreal generic replication is the MVP transport. `AHansaGameState` carries the small public campaign projection, `AHansaPlayerState` carries public house identity, and the owning `AHansaStrategyPlayerController` carries its private relevancy-filtered projection and command feedback.
- Client DTOs are purpose-built copies. City interest filters placement and markets. Route state is owner or spatially relevant, with cargo visible only to the owner. Research and detailed rejections are owner-only. Victory progress is public.
- Each client packet contains a server authoritative diagnostic hash and a separate digest of that partial client projection. These values have different meanings and must not be compared as though a client held full authoritative state.
- A client-reported stale projection revision triggers a full current snapshot plus retained relevant events. Matching revisions receive newly published relevant events. Refresh never pauses the authoritative simulation.
- A Development dedicated-server target exists for process-level proof. Shipping excludes developer automation.

## Consequences

Positive:

- Single-player and multiplayer use one mutation path and one deterministic ownership implementation.
- Private economy, research, cargo, and rejection detail are not broadcast through GameState.
- Generic Unreal replication is sufficient for the bounded MVP and keeps a projection abstraction in front of future transport changes.
- Late refresh and hash diagnostics can identify a stale client without exposing mutable full state.

Costs:

- Coarse projection arrays are resent as one owner-only snapshot property in the MVP.
- Interest is an explicit list of at most four scenario cities rather than a production spatial subscription service.
- The current server assigns the first two proof clients to the two authored houses.

## Compliance

`Hansa.Multiplayer.Authority.ServerValidatedProjections` must prove two principal registrations, accepted owner commands, cross-house rejection, duplicate and out-of-order rejection, global order across principals, city relevancy, owner-only research and cargo, public victory progress, ordered event deltas, stale-revision full refresh, matching authoritative hashes, distinct projection digests, reliable server RPC flags, and replicated client DTO flags.

The Hansa runtime module must never expose `FHansaSimulationState` through a reflected client property. New client-facing state must extend the versioned projection DTO and its relevancy/digest test.

## Deferred

- S11-P04 multi-process server/client launch, disconnect/reconnect, screenshots, correlations, and cleanup evidence.
- Platform sessions, lobbies, invites, teams, diplomacy, chat, matchmaking, migration, and NAT traversal.
- Prediction, rollback, latency compensation, bandwidth shaping, Replication Graph, and Iris evaluation.
- Delta serialization for coarse projection arrays after profiling demonstrates a need.
