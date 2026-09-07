# S11-P04 two-player authority and reconnect proof

## Fixture contract

`two_player_authority_v1` is a deterministic, paused multiplayer scenario with one authoritative process and two client processes. It uses campaign seed `91470745841716` and starts at tick zero. The fast compiled-registry contract test records `2b9e8957c51b7c0e` after its four-command sequence.

The server and clients launch with `-HansaAuthorityFixture`, explicit automation enablement, per-process named pipes, and one short-lived run token. Fixture mode pauses real-time simulation so process startup and reconnect timing cannot change the authoritative checksum. Explicit server-side automation stepping remains available through the normal simulation host.

## Process proof

Run:

```powershell
./Scripts/RunTwoPlayerAuthorityProof.ps1
```

The driver starts a headless Unreal authority process and two rendered Unreal client processes. It waits for authenticated automation readiness, two registered owners, replicated PlayerState readiness, and nonempty projection diagnostics before issuing commands.

Owner one places a road and is rejected when operating owner two's route. Owner two is rejected when operating owner one's route and then activates its own route. The proof requires both clients to converge on the server authoritative hash while retaining distinct projection digests and owner-only route cargo visibility.

The second client is terminated, the server must observe one remaining registration, and a fresh client process reconnects. The reconnect must reclaim the available second house, receive a full projection, preserve its private route view, and match the unchanged server authoritative hash.

The launched world loads the authored asset registry, while the fast contract test uses the compiled fallback registry. The process proof therefore records its own authoritative hash and requires the server, both clients, and the reconnect to agree on that value; it does not compare that asset-backed value with the compiled-registry golden hash.

## Evidence and cleanup

Each run writes a timestamped artifact directory beneath `Saved/BuildArtifacts` containing:

- one Unreal log and one launcher-stream log per server/client process;
- bounded request correlation IDs;
- readiness, command feedback, disconnect, reconnect, hash, and digest snapshots;
- native 1280x720 captures from both original clients;
- a native 1920x1080 capture from the reconnected client;
- screenshot metadata, semantic snapshot, gameplay query snapshot, and fixture metadata;
- `result.json` on success or `failure.json` on failure.

The Node driver uses a single overall timeout, detects early process exits, closes automation sessions and pipes, and terminates every child process in a `finally` block. Screenshots are copied byte-for-byte into the run artifact and are never resampled.

## Boundary

This is a loopback technical proof over Unreal's generic replication. It does not provide matchmaking, lobbies, invites, account identity, reconnect UI, durable session reservation, host migration, teams, diplomacy, chat, prediction, rollback, latency compensation, packet-loss simulation, NAT traversal, platform sessions, replication-graph tuning, or production security hardening.

The installed Epic Launcher Unreal Engine cannot build the dedicated `HansaServer` target. Local execution therefore uses UnrealEditor game/server processes; a server-capable engine distribution remains required to validate the dedicated-server binary.