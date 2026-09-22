# Hansa post-MVP multiplayer release contract

Status: approved implementation target, not evidence that multiplayer is released.  
Baseline date: 2026-09-19.  
Execution sequence: [MultiplayerImplementationPrompts.md](../../MultiplayerImplementationPrompts.md).  
Requirement evidence: [Acceptance.md](Acceptance.md).

## Release boundary

The multiplayer release is a Windows x64, Unreal Engine 5.8 campaign experience for two through eight human participants, with optional server-controlled AI houses. It preserves the authored game, simulation, and geography available at release time. It is a post-MVP milestone and does not change or retroactively satisfy the historical two-client MVP gate.

Supported release builds are a packaged Windows client with single-player and listen-host modes, plus a packaged headless `HansaServer` dedicated-server target. Client qualification targets Windows 11 23H2 or newer. Dedicated-server qualification targets Windows Server 2022 or newer and Windows 11 for development operation. A server-capable UE 5.8 toolchain is mandatory; an Editor `-server` process is test infrastructure, not the release server.

The release supports LAN discovery, direct address, and one internet service. It excludes console cross-play, ranked matchmaking, voice chat, multiple storefront integrations, and a 30–50-city content expansion.

## Campaign and participant contract

- A campaign has two through eight authored house slots. Each slot is `human`, `AI`, `open`, `closed`, or `reserved`; scenario validation may narrow the allowed states.
- Campaign, session, participant, house, and team identities are separate stable IDs. A connection, display name, provider account ID, or filename is never gameplay identity.
- A human controls exactly one separately owned house. Several humans controlling one house is out of scope unless a later explicit rule adds it.
- The server authenticates and binds a participant to a house. First-available-house assignment remains fixture-only.
- AI submits normal validated commands. Human takeover and AI replacement are atomic, policy-controlled transitions.
- Modes are competitive, authored teams, and cooperation between separately owned houses. Cooperation grants explicit permissions and does not merge ownership implicitly.
- Join policy may be public, friends, invite-only, direct/LAN, closed, or reserved. Late join and reconnect remain command-locked until authorized catch-up completes.
- Reconnect credentials are expiring, replay-resistant, and bound to participant, campaign, authority epoch, and provider identity where applicable. Display names are not credentials.

## Authority, privacy, and continuity

Only the active server advances simulation, decides time, accepts economic outcomes, assigns stable entity IDs, or writes campaign saves. Single-player, AI, human RPCs, and controlled automation use the same command gateway. Clients may compute previews but may not mutate authoritative state locally.

Public, owner, team, city, vehicle, and authorized-report projections are versioned and bounded. Subscriptions are reauthorized, revoked data is removed, and a client digest is compared only with the server's equivalent authorized projection at the same revision. The global authoritative hash is diagnostic metadata, not proof that a partial view is correct.

The server owns named campaign saves, autosaves, retention, migration, and resume. Portable gameplay state contains participant bindings and rules but no reusable login token. The current fixed `manual.hansa` and `autosave.hansa` files are baseline-only.

Local menus never pause multiplayer. Pause and speed use authored host-only or vote policies with real-time timeouts independent of simulation time. Single-player retains immediate authority through the same policy surface.

A graceful listen-host departure commits a checkpoint and transfers to one designated successor with a new authority epoch. Abrupt host loss recovers from the latest committed checkpoint and reports its age and possible lost progress. This is not seamless migration. Becoming an authority is an explicit trust transition because it can inspect all gameplay state. Old-epoch traffic and split-brain continuation are rejected.

## Internet provider decision

Epic Online Services is selected behind Hansa-owned asynchronous authentication and session adapters. The shipping-oriented integration is **Online Subsystem EOS**, not the still-Beta Online Services API. EOS supplies Epic Account Services/Connect identity, Sessions/Lobbies, invites/presence where supported, and EOS P2P relay/NAT traversal. LAN/direct uses a separate offline trust model.

The choice is storefront-neutral: the Windows build need not launch from Epic Games Launcher. Account Portal is the default internet login; later storefront linking stays behind the adapter. Product registration, deployment IDs, client policy, credentials, and real accounts remain required before MP-10 live acceptance. Secrets must not enter source control or client logs.

Sources checked 2026-09-19:

- [UE 5.8 Online Subsystem EOS](https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine) documents the plugin, registration, interfaces, and Account Portal without a launcher requirement.
- [UE 5.8 Online Services overview](https://dev.epicgames.com/documentation/unreal-engine/overview-of-online-services-in-unreal-engine) directs near-term shipping titles to Online Subsystem.
- [EOS multiplayer](https://onlineservices.epicgames.com/multiplayer?lang=en-US) documents Sessions, Lobbies, and P2P firewall/NAT connectivity.

No EOS, Steam, PlayFab, Photon, GDK, or other online integration was detected. MP-09 owns provider-neutral lifecycle contracts; MP-10 owns EOS and live proof.

## Proposed performance gates

These are pre-optimization budgets for MP-22, not MP-01 measurements. They apply with eight humans, authored AI fill, live production/logistics/research, 2,000 buildings, 64 active routes, 20,000 inventory rows, and ordinary UI subscriptions.

| Metric | Release budget |
| --- | --- |
| Server simulation/command/projection work | p95 <= 25 ms and p99 <= 40 ms per step; no healthy-network step > 100 ms |
| Dedicated-server working set | <= 4 GiB after warm-up; <= 10% growth from hour one to hour eight |
| Sustained server egress | <= 64 KiB/s average and <= 128 KiB/s p95 per client, excluding join snapshots |
| Sustained client ingress | <= 96 KiB/s average and <= 192 KiB/s p95, excluding join snapshots |
| Command feedback | p95 <= 150 ms LAN, <= 400 ms at 150 ms RTT, <= 750 ms at 300 ms RTT |
| Authorized initial snapshot | <= 8 MiB compressed and <= 10 s to interactive at 10 Mbit/s, 150 ms RTT |
| Reconnect | <= 15 s at 10 Mbit/s, 150 ms RTT when the delta window is available |
| Save checkpoint | p95 <= 2 s without stopping network/session timeouts; atomic write and fallback required |
| Eight-hour soak | no corruption, conservation failure, unbounded queue, or memory growth above budget |

MP-22 reports percentiles and worst cases under 0/50/150/300 ms RTT, jitter, 1/3/5% loss, outages, and constrained bandwidth. Severe cases may time out explicitly but may not corrupt authority.

The MP-01 evidence machine was Windows 11 Pro build 26200, Intel Core i9-13900K (24 cores/32 logical processors), 127.7 GiB RAM, and NVIDIA GeForce RTX 5060 Ti, using UE 5.8. This is provenance, not minimum hardware. MP-22 must add minimum-client and production-server baselines.

## Current gameplay action audit

Normal player-facing mutations found in this checkout follow. Test-only entity/no-op commands are excluded. Road drawing and compound placement submit placement commands. Route load/unload terms belong to route plans; there is no separate spot-market buy/sell player command.

| Player action | Authoritative path now | Network intent now | Owner prompt |
| --- | --- | --- | --- |
| Place building by click/drag | `PlaceBuilding` | `PlaceBuilding` | MP-05 |
| Draw road/held stroke | batch `PlaceBuilding` | individual placement only | MP-05 |
| Cancel construction | `CancelConstruction` | missing | MP-05 |
| Demolish building | `RemoveBuilding` | missing | MP-05 |
| Upgrade residence | `UpgradeResidence` | missing | MP-05 |
| Pause/resume production | `SetProductionActive` | missing | MP-05 |
| Change recipe/mode | `SetProductionMode` | missing | MP-05 |
| Upgrade production | `UpgradeProduction` | missing | MP-05 |
| Set heating reserve/protection | `SetHeatingReserve` | missing | MP-05 |
| Change household availability | `SetHouseholdAvailability` | missing | MP-05 |
| Create route/assign vehicle | `CreateRoute` | missing | MP-05 |
| Edit stops/load/unload/quantity/reserve | `EditRoute` | missing | MP-05 |
| Start/pause route | `SetRouteActive` | `SetRouteActive` | MP-05 |
| Cancel route | `CancelRoute` | missing | MP-05 |
| Queue research | `QueueResearch` | `QueueResearch` | MP-05 |
| Issue manual ship destination | `MoveShip` | missing; local runtime call | MP-05 |
| Select/inspect game objects | presentation read from local host | incomplete remote projection | MP-06 |
| Change pause/speed | HUD writes local runtime | no session policy/RPC | MP-12 |
| New game/begin/resume | local runtime/presentation | no session lifecycle | MP-09/11/12 |
| Manual save/load/autosave | local `UHansaSaveSubsystem` | no server campaign operation | MP-13 |

The authoritative envelope has 15 production command variants; `EHansaClientIntentType` carries only three. MP-05 must refresh this inventory against its checkout.

## Current query and presentation audit

Existing controlled queries are coverage inputs, not automatically safe client APIs:

- strategic/fixture: `fixture.summary`, `strategic.summary`, `strategic.evidence`, `building.list`, `building.market_access`, `research.state`, `scenario.progress`, `ai.decision_history`, `market.alerts`, `market.diagnosis`;
- construction/production: `construction.list`, `construction.get`, `construction.cost`, `production.list`, `production.get`;
- logistics/inventory: `inventory.stock`, `inventory.spoilage`, `logistics.requests`, `logistics.jobs`, `logistics.path`;
- population/cities: `city.population`, `population.cohort`;
- routes/vehicles: `route.list`, `route.get`, `route.cargo`, `route.events`, `vehicle.list`;
- market: `market.opportunity`, `market.report_age`, `market.known_price`, `market.known_components`, `market.price`, `market.components`, `market.history`, `market.reserve`, `market.explanation`, `market.consumers`, `market.producers`;
- multiplayer fixture: `multiplayer.status`, `multiplayer.request_refresh`.

Current client snapshots expose placements, current market stock/reserve/price, routes with owner-gated aggregate cargo, owner research, public victory objectives, filtered events, owner money, and scenario outcome. MP-06 must add or honestly mark unavailable: construction causes/cost; road/service topology; production inputs/outputs/recipe/utilization/blockers/upgrades/batches; inventories/reservations; population/needs/workforce/satisfaction; market demand/production/incoming/history/causes/report knowledge; logistics jobs and visible cargo; route stops/transfers/schedules/navigation/manual orders; technology definitions/effects; scenario detail; alerts; saves/sessions; and permission/stale explanations.

## Baseline structural findings

- `AHansaGameMode::PostLogin` chooses only `GetHouseId()` then `GetRivalHouseId()`; a third connection cannot get a distinct house. Rival AI suspension is coupled to house two.
- Runtime validation expects one player plus an optional rival (`Snapshot.Players.Num() == 1 or 2`); tests/fixtures assert two house and research records.
- HUD and presentation models call `UHansaRuntimeSimulationHost::BuildProjection`, `GetHouseId`, mutation helpers, and `SetSpeed` directly. `UHansaBuildMenuPresentationModel` can create `StandaloneBuildRuntime`.
- `AHansaStrategyPlayerController::HandleShipMoveIntent` submits to the local runtime; there is no ship network intent.
- Save discovery is exactly local `manual.hansa` and `autosave.hansa`. There is no named server campaign, participant binding, retention, resume lobby, or network authorization.
- Fixture reconnect releases a house and gives the next process the available second house. It has no durable identity, reservation, credential, grace period, or uncertain-command reconciliation.
- The projection omits the presentation data listed above, while many real screens depend on local full simulation access.

These are baseline gaps, not implementation claims. Their prompt and verification ownership is in [Acceptance.md](Acceptance.md).
