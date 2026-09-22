# Multiplayer requirement-to-evidence matrix

This is the normative post-MVP matrix. `Implemented` means code/content exists; `Verified` requires the listed test and retained evidence. Design prose alone never advances a row.

| ID | Requirement | Prompt | Required verification/evidence | Current state |
| --- | --- | --- | --- | --- |
| MP-R001 | Stable campaign/session/participant/house/team IDs; 2–8 authored slot states | MP-02 | model/schema/migration/save tests for 2–8 and invalid bindings | Implemented and verified for MP-02: four model tests plus scenario/schema/catalog tests; see [MP-02.md](MP-02.md). Eight-player authored geography remains MP-04. |
| MP-R002 | Server-evaluated ownership, sharing and public/private/team policy | MP-02/06/16 | role matrix tests proving fields/actions and revocation | MP-02 policy implemented and verified deny-by-default for owner/team/public/explicit grants. Projection enforcement and revocation remain MP-06/16. |
| MP-R003 | Authenticated admission, compatibility, reservations, bans and replay resistance | MP-03 | forged/expired/reused/duplicate/full/incompatible suite | Implemented and verified provider-neutrally: three admission tests plus authority projection denial; see [MP-03.md](MP-03.md). Live EOS belongs to MP-10. |
| MP-R004 | Explicit LAN trust; display names are not authentication | MP-03/09 | LAN credential and spoofing integration tests | MP-03 credential/trust model implemented and verified: display-name-only join is denied and server-issued credentials expire, rotate, and reject replay. Production LAN transport remains MP-09. |
| MP-R005 | 2–8 human houses and atomic optional AI fill/replacement | MP-04 | 2/4/8-client, AI mix, ninth-client and double-claim tests | Implemented and verified at server-authority/runtime level; see [MP-04.md](MP-04.md). Packaged multi-process proof remains MP-09/14/24. |
| MP-R006 | Every current player mutation uses the validated server gateway | MP-05 | remote-control and semantic-action case for every action audit row | Implemented and verified at authority/control-wiring level for all 15 current mutations; see [MP-05.md](MP-05.md). Packaged remote UAT remains MP-09/23. |
| MP-R007 | Bounds, authorization, idempotency, stale rules, fairness and feedback | MP-05/21 | duplicate/reordered/concurrent/unauthorized/flood tests with conservation | MP-05 command bounds, ownership, replay/order, stale-tick, atomic-batch and structured-feedback gates verified. Flood/rate hardening remains MP-21. |
| MP-R008 | Bounded role projections support every normal screen | MP-06 | per-role DTO tests, remote UAT, byte/cost measurements | Implemented and verified at authority/replicated-cache level: complete version-2 DTOs, bounded history/deltas, 170,804-byte initial view and 2.465 ms measured build; see [MP-06.md](MP-06.md). Packaged full-screen UAT remains MP-09/11/23. |
| MP-R009 | Revocation clears private cache; equal authorized views converge | MP-06/08 | membership/interest revocation and resync tests | MP-06 explicit stable-ID eviction, full-refresh replacement, server-only report grants and authorized-view digests implemented and verified. Divergence-triggered resync remains MP-08. |
| MP-R010 | Chunked late join and ordered catch-up without global pause | MP-07 | loss/duplicate/order/cancel/overflow/permission multiprocess suite | Not implemented |
| MP-R011 | Secure reserved reconnect and uncertain-command reconciliation | MP-08 | before/after acceptance, concurrent/expired reconnect tests | Not implemented; fixture reacquisition only |
| MP-R012 | Async provider-neutral sessions with LAN/direct listen/dedicated lifecycle | MP-09 | repeated create/discover/join/leave/destroy/cancel packaged tests | Not implemented |
| MP-R013 | EOS identity, sessions/lobbies, invites/presence and P2P relay | MP-10 | mock adapter plus real different-network accounts/outage tests | Provider selected; credentials absent |
| MP-R014 | Accessible frontend, browser, host/lobby, loading and reconnect UX | MP-11 | packaged mouse/keyboard/controller UAT at required scales | Not implemented |
| MP-R015 | Host/vote pause/speed policy and real-time session timeouts | MP-12 | 2/4/8-player tie/timeout/disconnect/join/save/solo tests | Not implemented |
| MP-R016 | Named server saves, atomic retention/migration/recovery and resume lobby | MP-13 | terminate/resume comparison and failure suite | Fixed local slots only |
| MP-R017 | Packaged `HansaServer`, operation, admin and backup/restore | MP-14 | headless eight-client restart/resume and Shipping scan | Blocked: server-capable UE toolchain |
| MP-R018 | Graceful handoff and honest checkpoint crash recovery; one epoch | MP-15 | leave/crash/partition/stale/split-brain/successor tests | Not implemented |
| MP-R019 | Competitive/team/co-op with separate ownership and explicit grants | MP-16 | complete campaigns, revocation/tie/reconnect/save/editor tests | Not implemented |
| MP-R020 | Bounded channels, mute/block, moderation and visibility-safe pings | MP-17 | forged recipient/reference, spam, reconnect, input tests | Not implemented |
| MP-R021 | Physical trade/delivery contracts preserve value and obligations | MP-18 | fulfillment/race/expiry/default/save/conservation tests | Not implemented |
| MP-R022 | Joint venture and agreement permissions preserve ownership/proceeds | MP-19 | multi-house lifecycle/race/default/save/team tests | Not implemented |
| MP-R023 | Recovery finance and anti-grief rules conserve goods/currency | MP-20 | collateral/integer/save/abuse and ruleset tests | Not implemented |
| MP-R024 | Every network entry point is bounded/authorized and redacts credentials | MP-21 | malformed/replay/oversize/bomb/flood/admin suite and scan | Partial authority checks |
| MP-R025 | Performance and soak budgets in ReleaseContract pass | MP-22 | declared-hardware adverse-network and eight-hour reports | Budgets proposed only |
| MP-R026 | Full packaged journey and authoring parity | MP-23 | multi-machine UAT, semantic evidence, captures, schema audit | Not implemented |
| MP-R027 | Clean client/server release reproduces all mandatory gates | MP-24 | clean report, provider smoke, backup/restore, fresh join | Not implemented |
| MP-R028 | Single-player remains on common authority and passes its journey | MP-05/12/24 | single-player regression and packaged golden journey | MP-05 standalone typed-gateway regression verified; pause/save and packaged golden journey remain MP-12/24. |
| MP-R029 | Declared Windows client/listen/dedicated OS/build support | MP-14/24 | packaged Windows 11 and Server 2022+ matrix | Not verified |
| MP-R030 | Editor/worker/credentials/staging/tests/automation absent from Shipping | MP-14/21/24 | target/package/dependency/content/secret scans | Release scan pending |

## Prompt gate ledger

| Prompt | Gate state | Evidence/report |
| --- | --- | --- |
| MP-01 | Verified with one recorded baseline test failure | [MP-01.md](MP-01.md) |
| MP-02 | Verified | [MP-02.md](MP-02.md) |
| MP-03 | Verified | [MP-03.md](MP-03.md) |
| MP-04 | Verified at server-authority/runtime level | [MP-04.md](MP-04.md) |
| MP-05 | Verified at authoritative command/control-wiring level | [MP-05.md](MP-05.md) |
| MP-06 | Verified at authority and replicated-cache level | [MP-06.md](MP-06.md) |
| MP-07–MP-24 | Not run | Add one `MP-XX.md` per execution; never infer completion from this matrix |

## Explicit exclusions

Deferred unless separately authorized: console cross-play, voice chat, ranked matchmaking, multiple storefront integrations, shared control of one house by several humans, full 30–50-city content, new combat, and complete historical League politics. Current-game features remain in command/projection scope even when added after MP-01; MP-05 and MP-23 refresh the action inventory.
