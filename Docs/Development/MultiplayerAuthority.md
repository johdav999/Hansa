# S11-P03 server authority and replicated projections

## Runtime boundary

The authoritative `UHansaRuntimeSimulationHost` exists only on the server or standalone authority. Network clients submit the bounded `FHansaClientCommandIntent` DTO through a reliable server RPC. The DTO never contains price, cost, ownership, authoritative tick, global sequence, generated entity ID, elapsed time, or resolved output.

`FHansaMultiplayerAuthority` binds each authenticated principal to exactly one existing house. It validates schema, size, stable IDs, nonce replay, exact per-client ordering, and principal registration before asking the runtime host to construct a normal `MultiplayerRpc` gameplay command. The host supplies command IDs, building IDs, authoritative tick, and global order. Gameplay ownership and resource rules remain in the normal deterministic command gateway.

A valid request consumes its per-client sequence even when gameplay rejects it. Malformed, duplicate, and out-of-order requests do not mutate simulation state. Owner-only feedback returns a stable rejection category, gateway cause, message, remedy, authoritative tick, and accepted global sequence.

## Replication model

`AHansaGameState` replicates the small public campaign projection: server tick, victory progress, scenario outcome, and the server authoritative diagnostic hash. `AHansaPlayerState` replicates public house identity and readiness.

Each `AHansaStrategyPlayerController` carries one owner-only `FHansaClientProjectionSnapshot`. Player controllers are only relevant to their owning connection, so private money, research, route cargo, command feedback, and the client's filtered event stream are not broadcast through GameState.

The client snapshot is a purpose-built DTO. It contains:

- placements for interested cities;
- market summaries for interested cities;
- owned routes plus spatially relevant public route state;
- cargo only for the route owner;
- research only for the owning house;
- public scenario and victory progress;
- relevancy-filtered ordered events.

It contains no mutable authoritative containers, inventories, command queue, RNG state, save bytes, definition registry, AI private state, or writable pointer.

Each packet distinguishes the server authoritative hash from the digest of the filtered client projection. A stale client revision triggers a full projection refresh; an up-to-date revision receives only events newer than its last delivered server event sequence. The authoritative simulation never pauses for refresh.

## Unreal integration

`AHansaGameMode` owns the authority coordinator, assigns the first two connections to the available player and rival MVP houses, suspends the rival AI while a human owns that house, refreshes projections after commands and authoritative ticks, and unregisters disconnecting principals. The strategy player controller exposes reliable server intent and interest RPCs and a reliable owner-only feedback RPC.

`HansaServer.Target.cs` provides the dedicated-server target. Development servers include the developer-only automation module needed by S11-P04; Shipping servers exclude it.

The installed Epic Launcher Unreal Engine 5.8 distribution used for local verification reports that Server targets are unsupported. The target is checked in for a server-capable source or installed engine build; the locally executable proof is the successful Development game/listen-server build together with the isolated Editor link and multiplayer automation test.

The proof deliberately omits production matchmaking, lobby/invite UX, diplomacy, teams, chat, prediction, rollback, latency compensation, Replication Graph, and platform session integration.

## S11-P04 launched-process proof

The checked-in `two_player_authority_v1` fixture and `RunTwoPlayerAuthorityProof.ps1` launch one authority and two clients, exercise accepted and rejected owner commands, compare the server hash with each partial projection digest, terminate one client, and require a full reconnect projection. Each process has an authenticated pipe and separate logs; rendered clients produce correlated native-resolution evidence. See [TwoPlayerAuthorityFixture.md](TwoPlayerAuthorityFixture.md).
