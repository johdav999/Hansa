# Hansa — Full multiplayer implementation prompts

Prepared 2026-09-19. This is an executable prompt sequence, not a claim that the features exist. Run prompts in order. Each execution implements its bounded task, verifies it, and records evidence before advancing.

## Outcome and scope

Deliver a player-facing Windows multiplayer release for 2–8 human players, optional AI houses, competitive campaigns, teams, and cooperation between separately owned houses. Support listen hosts and packaged dedicated servers, LAN/direct connections and one real internet identity/session/relay provider, discovery and invites, late join, reconnect, server-owned saves, campaign resume, communication, administration, and recoverable host failure.

Use the current authored game and geography. Multiplayer must cover every currently shipped gameplay action, including features added after the original MVP. This milestone does not automatically require the full 30–50-city campaign, new combat, all historical politics, console cross-play, voice chat, ranked matchmaking, or multiple storefront integrations. Prompt MP-01 records the precise boundary and dependencies. Cooperation means separate houses with explicit sharing and trade permissions; controlling one house with several humans is not implicitly included.

The existing MVP retains its historical two-client scope. This is a distinct post-MVP expansion requested by the user. Do not silently rewrite old acceptance evidence as proof of this milestone. Contracts, loans, joint ventures, and recovery below are bounded multiplayer systems, not permission to implement the entire final economy.

## Use

Paste this launcher, replacing the ID, into an implementation task:

```text
Read Docs/MultiplayerImplementationPrompts.md. Execute MP-01 in full, including its shared execution contract and acceptance gate. Implement and verify the work; do not stop at a plan. Preserve unrelated working-tree changes. Report exact evidence and any remaining blockers. Do not execute later prompts in this turn.
```

Continue with MP-02, MP-03, and so on. A prompt is complete only when its gate passes. If a prerequisite is blocked, record it and complete independent work without presenting a mock, skipped check, or disconnected screen as working production multiplayer.

## Shared execution contract — applies to every prompt

1. Read repository instructions, `Docs/MVP.md`, `Docs/TechnicalArchitecture.md`, `Docs/EditorArchitecture.md`, and the relevant existing implementation notes. Inspect current code and working-tree changes before editing. Build on `FHansaMultiplayerAuthority`, `UHansaRuntimeSimulationHost`, `AHansaGameMode`, the strategy controller, replicated projections, and existing tests rather than creating a competing authority system.
2. Maintain single-player, AI, human network commands, and controlled automation on the same validated command gateway. Only the server advances simulation or accepts economic outcomes. Never trust client-supplied house ownership, prices, resource availability, time, entity IDs, or outcomes.
3. Every changed gameplay model ships with stable IDs, deterministic serialization/hash coverage, save migration, editor schema/metadata, validation, import/export, impact analysis, and applicable generation schema support. Do this in the feature prompt, not in a later cleanup. Keep editor and generation-worker dependencies out of runtime. Third-party online SDKs belong behind the runtime session adapter; they are distinct from editor-only media-generation providers.
4. Each player-visible action has stable semantic automation IDs, authorized queries/actions, structured cause/remedy errors, keyboard/controller access, and real viewport evidence. Automation cannot bypass identity, permissions, or gameplay validation. Normal CI uses mocked online services; live provider checks use the configured test environment and bounded resources.
5. Before designing or materially changing any GUI, read `design.md`, `Docs/UIDesignBrief.md`, and `Docs/UIAssetWorkflow.md` completely and use the `imagegen` skill. Inventory shell, navigation, panels/lists, controls, feedback, overlays/charts, icons, and decoration; mark non-applicable categories explicitly. Define default, hover, pressed, selected, disabled, focus, loading, unavailable, warning, and error states as applicable. Generate composed references and separate reusable component references/assets. Reuse approved assets; generate each new distinct image/icon separately. Save masters and sibling prompt records, inspect original and actual display sizes, and obey the approved GUI-only proportional-resizing exception. Implement native Slate/UMG screens with dynamic native text, existing style tokens, and approved individual artwork. No mockup-as-screen or handmade substitute icons.
6. Preserve privacy in actual transmitted data, not just widgets. Compare client projection digests only against the server's equivalent authorized projection at the same revision. An echoed authoritative hash is diagnostic metadata, not proof that a partial client view is correct.
7. Use meaningful domain, integration, multi-process, and packaged-build tests appropriate to the change. Record failures honestly; do not repin golden hashes or relax assertions solely to obtain a pass. Include reproducible commands, build/content/protocol identifiers, relevant logs, measured results, and screenshots when visual behavior changes.
8. Write each report to `Docs/Development/Multiplayer/MP-XX.md` and update a requirement-to-evidence matrix at `Docs/Development/Multiplayer/Acceptance.md`. Track implemented, verified, blocked, and deferred separately. Keep generated content staged with provenance until its actual diff/assets receive required promotion approval. Complete reviewable work before requesting approval.

## Sequence

| Stage | Prompts | Exit |
| --- | --- | --- |
| Foundations | MP-01–MP-05 | Eight correctly authorized participants and complete gameplay command coverage |
| Synchronization | MP-06–MP-08 | Private, bounded projections; live joins and secure reconnect |
| Player sessions | MP-09–MP-12 | Real frontend, lobby, internet sessions, and pause/speed rules |
| Campaign continuity | MP-13–MP-15 | Saved campaigns, dedicated servers, host recovery |
| Social gameplay | MP-16–MP-20 | Teams, communication, enforceable agreements and recovery |
| Release proof | MP-21–MP-24 | Adverse-network, security, performance, packaged acceptance |

### MP-01 — Establish the release contract and current baseline

Audit the current checkout, not just the September 11 two-client result. Run the existing authority/reconnect tests and process runner, record current failures, and inventory all normal gameplay actions and query surfaces. Identify hard-coded first/second player assumptions, standalone-only paths, client access to local simulation, save-slot assumptions, and presentation data missing from projections.

Create a post-MVP multiplayer specification and acceptance matrix. Define supported OS/builds, 2–8-player capacity, modes, house assignment, AI replacement, join/reconnect, pause/speed, saves, hosting, online services, ownership/privacy, and host-loss behavior. Link it from MVP documentation as a separate future milestone without changing historical MVP gates. Record proposed performance budgets and hardware/workload baselines before optimizations. Detect existing online integrations; choose one provider using current official documentation and record the decision. If a storefront/account decision is genuinely missing, ask only for that external choice while completing the audit and provider-neutral work.

Acceptance: every release requirement maps to a prompt and test; each existing gameplay action appears in the audit; baseline reports distinguish old evidence from checks run now. No implementation claim based only on design prose.

### MP-02 — Versioned session, participant, and permissions model

Implement separate stable campaign, session, participant, house, and team identities. Model human/AI/open/closed/reserved slots, participant lifecycle, host/admin roles, per-house permissions, content/protocol compatibility, and authored scenario slot rules. Connections and provider account identifiers must not become gameplay identity. Define ownership, shared access, and public-information rules as server-evaluated policies. Add deterministic scenario validation, editor authoring, migrations, and fixture coverage.

Acceptance: model tests cover 2–8 slots, invalid/duplicate bindings, team constraints, save round trips, and supported old content. Runtime simulation has no dependency on an online-service SDK or editor module.

### MP-03 — Real identity binding and secure admission

Replace first-available-house assignment as the production admission policy. Bind a server-validated online identity to a participant and authorized house. LAN/offline mode must have an explicit separate trust model and server-issued reconnect credentials; it must not pretend display names authenticate a player. Validate version, scenario, definition/content manifest, capacity, reservation, ban state, and join policy before releasing private data. Add bounded admission timeouts, replay-resistant credentials, duplicate-login handling, redacted diagnostics, and structured failures. Test against an authentication adapter mock until the selected live provider is integrated.

Acceptance: spoofed house/account, expired/reused credentials, incompatible content, duplicate login, full sessions, and rejected joins cannot acquire a slot or receive private projections. Credentials do not appear in logs or client-visible public metadata.

### MP-04 — Generalize the simulation to eight houses

Remove two-house assumptions in game mode, scenario initialization, command routing, AI ownership, house queries, scoreboards, and identifiers. Support 2–8 human participants and scenario-bounded AI replacements/fill slots. Author valid starting assets and buildable opportunities for each house using current geography; validate finite resources and non-overlapping placements. AI must relinquish control atomically when an authorized human takes over and resume only according to the session policy.

Acceptance: eight independent clients own distinct houses; a ninth is rejected; simultaneous joins cannot double-claim a house; human/AI swaps never duplicate assets or execute competing control streams. Deterministic tests cover zero AI, mixed AI/human, and replacement transitions.

### MP-05 — Complete all player command paths

Use MP-01's inventory to route every current player mutation through server intents: construction and held strokes, roads, demolition/cancellation, population upgrades, production pause/recipe changes, market trades, route editing and operation, manual ship orders, research, scenario actions, and newly discovered current features. Preserve standalone behavior. Add per-command bounds, authorization, sequence/idempotency handling, stale revision rules, fairness under concurrent commands, and actionable pending/accepted/rejected UI feedback. Presentation previews remain local and cannot spend resources.

Acceptance: execute every inventory row from a remote client through normal controls and through its semantic action. Concurrent purchases, overlapping builds, repeated packets, recipe changes, and duplicate route commands cannot double-spend, duplicate goods, or partially mutate state. No shipping action silently calls a standalone-only mutation.

### MP-06 — Complete private and relevant projections

Replace incomplete or oversized proof snapshots with versioned, bounded public, owner, team, city, vehicle, and authorized-report projections. Cover every shipped inspector, HUD metric, market history, population need, construction state, production batch, cargo movement, ship navigation, research, and outcome. Keep histories paged and collections delta-updated. Client world actors reconstruct cosmetic presentation from authorized state without needing the simulation host. Recheck permissions on every subscription and remove previously visible cached private state when access is revoked.

Acceptance: per-role projection tests prove absent forbidden fields; changing city/team/ownership revokes data and stale UI; world streaming does not stop server simulation. All normal screens function remotely with honest unavailable/stale states. Measure serialized bytes and refresh costs.

### MP-07 — Join a running campaign without pausing it

Implement the architecture's bounded chunked snapshot and ordered delta catch-up protocol with snapshot tick/revision, integrity checks, acknowledgments, memory limits, cancellation, retry, and timeout. Buffer only an explicitly bounded delta window; restart safely if the client falls behind it. Apply a complete validated baseline atomically, then ordered deltas, then unlock commands. Use per-view digests rather than comparing a private subset to the full simulation hash.

Acceptance: a late client joins while the other players keep constructing, trading, researching, and sailing. Loss, duplicate/out-of-order chunks, interrupted transfer, permission changes, and catch-up overflow cause safe recovery with no mixed revisions, leaks, duplication, or global pause. Demonstrate nonzero advancing server ticks throughout join.

### MP-08 — Reconnect, desynchronization, and command uncertainty

Implement secure reserved-seat reconnect across new client processes, configurable grace periods, server-approved AI takeover, expiring reservations, and explicit rejoin failures. Preserve command idempotency across connection epochs so a command accepted before disconnect is neither replayed nor silently lost in the UI. Detect authorized-projection divergence, rate-limit resync, and rebuild clients through MP-07. Define handling when the house is defeated, banned, reassigned, or the campaign changes during absence.

Acceptance: reconnect restores the correct house and current authorized state while the simulation advances; another identity cannot steal the seat. Test disconnect before/after command acceptance, concurrent reconnect attempts, expired reservations, repeated resync, and changed membership. Preserve a bounded, privacy-filtered absence summary for the later UI.

### MP-09 — Session lifecycle and real LAN/direct hosting

Implement a project-owned asynchronous session adapter with create, discover, join, leave, destroy, readiness, cancellation, timeout, and error contracts. Provide working LAN discovery and direct-address joins for listen and dedicated modes. Separate frontend lobby state from campaign state and clean up on travel, failure, logout, and shutdown. Avoid stale callbacks reviving cancelled sessions. Keep address/password handling bounded and private.

Acceptance: two packaged or Development game processes on separate machines can discover/join, leave, rehost, and reconnect without console commands or automation credentials. Test repeated lifecycle cycles, occupied ports, disappearing hosts, failed travel, and cancel races. Document actual firewall/connectivity requirements.

### MP-10 — Selected internet provider, invitations, and relay

Integrate the single provider selected in MP-01 behind the session/authentication adapters. Verify supported APIs and engine compatibility against current official documentation. Implement real login/logout, authenticated join, public/private/friends visibility where supported, search filters, invitations, presence, relay/NAT traversal, service-outage behavior, and session advertisement cleanup. Keep privileged server credentials outside clients and source control. Add deterministic adapter contract tests; do not substitute mock success for live completion.

Acceptance: independently authenticated clients on different networks discover or accept an invite and play through the actual transport. Test invitation while running/in frontend, expired invitations, access restrictions, provider outage, and account switching. Record provider configuration and real test evidence with secrets redacted. Missing external credentials block live acceptance only; state that explicitly.

### MP-11 — Complete frontend, browser, lobby, and connection UI

Apply the shared GUI/ImageGen contract. Build native multiplayer navigation, host setup, browser/search/filter, direct join, invitations, lobby roster, house/team selection, AI/open/closed slots, scenario/rules summary, ready/start/cancel, loading, compatibility errors, disconnect/reconnect, and return-to-menu flows. Server owns lobby decisions; rule changes invalidate readiness. Prevent start races, unauthorized edits, duplicate starts, and late callbacks. Reconnect uses MP-08 and shows the privacy-filtered absence summary.

Acceptance: mouse, keyboard, and controller users can start, find, join, configure, play, leave, and rejoin without developer tools. Verify states and focus at supported resolutions, 80–140% UI scale, large text, high contrast, and long localized labels. Capture actual assembled screens and list selected asset/prompt paths.

### MP-12 — Shared time, pause, speed, and session policy

Implement authored host-only and vote-based pause/speed policies with deterministic tie, timeout, disconnect, and quorum rules. Keep simulation time separate from real-time network/admission/vote timeouts so a paused game still reconnects and resolves session operations. Local menus never pause all players. Add visible policy, voter eligibility, results, and permitted actions to the existing HUD using the GUI contract.

Acceptance: two, four, and eight players can resolve pause/speed changes without desynchronization or indefinite pause abuse. Test host disconnect, electorate changes, repeated requests, pause during join/save, and solo compatibility. Commands at the same tick retain deterministic ordering.

### MP-13 — Authoritative campaign saves and resume

Extend the existing save system to named multiplayer campaigns, atomic manual/autosave writes, retention, compatible-version migration, and corruption recovery. Capture immutable state at a tick boundary; persist gameplay state, participant-to-house bindings, teams, rules, AI control, and pending deterministic obligations. Separate account association records from portable gameplay data; never save reusable login tokens. Restore through a resume lobby that authenticates returning participants, validates peer content, and explicitly authorizes any reassignment.

Acceptance: save during active trade/construction/research, terminate all processes, resume, and compare deterministic continuation and authorized projections. Test interrupted writes, disk failure, corrupt latest autosave, old saves, incompatible peers, missing participants, and unauthorized reassignment. Clients cannot rewind or overwrite the shared campaign.

### MP-14 — Packaged dedicated server and operation

Build and run the real `HansaServer` target with a server-capable engine distribution; an Editor `-server` process is not sufficient acceptance. Add versioned configuration, scenario/save selection, authentication/session registration, port binding, health/readiness, graceful shutdown, autosave, log rotation, resource limits, and authenticated administration. Implement kick/ban/unban and admission control without exposing automation as an internet administration API. Document reproducible build/deployment and backup/restore on the supported Windows host.

Acceptance: a packaged headless server boots, accepts eight clients, persists/resumes a campaign after restart, and drains/shuts down cleanly. Verify no UI/presentation dependency is required for authority. Missing server-capable toolchain is a named release blocker, not a reason to waive the binary test.

### MP-15 — Listen-host departure and failure recovery

Implement graceful host handoff with a final consistent checkpoint, reconnect instructions, and a single new authority epoch. For abrupt host loss, implement explicit recovery from the latest committed recovery checkpoint; show the checkpoint age and possible lost progress rather than claiming seamless migration. Design an ADR for secure checkpoint custody: ordinary competing clients must not gain every house's private state merely to enable recovery. A designated successor becoming authority must be an explicit trust transition. Reject old-authority traffic and prevent split-brain continuation; authentication/session ownership must move consistently.

Acceptance: graceful leave resumes without duplicated accepted commands; crash recovery resumes from the reported checkpoint with correct ownership. Test successor failure, duplicate takeover attempts, partition/rejoin, stale tokens, and unavailable checkpoints. Dedicated-server crash recovery continues to use MP-13/14. Document the unavoidable listen-host trust model and measured recovery behavior.

### MP-16 — Competitive, team, and cooperative campaigns

Implement the authored modes with explicit membership, shared visibility, optional resource/access permissions, team research policy, scoring, victory, surrender, and post-result behavior. Keep separate-house ownership unless a rule explicitly grants a capability. Define treatment of joiners, leavers, defeated members, AI replacements, and team changes; competitive defaults cannot expose ally or opponent private state by accident. Integrate native lobby, ownership badges, inspector permission explanations, and results using the GUI contract.

Acceptance: complete a competitive game, team game, and cooperative objective with real clients. Verify shared victory and individual ownership, deterministic tie resolution, revocation, save/reconnect, and denial of destructive actions without permission. All new rule data is editable and validated in Authoring Studio.

### MP-17 — Chat, pings, and player administration

Implement bounded global/team/direct text channels, mute/block, session moderation, and optional map/object pings. Validate recipients and object visibility server-side; hidden cargo/routes cannot leak through links, previews, chat history, or pings. Apply rate, length, history, and attachment limits; treat text as plain untrusted content. Add accessible native communication and player-list surfaces, sender identity, clickable authorized references, and session kick/ban feedback. Voice chat is outside this milestone.

Acceptance: restricted channels and object references remain private under forged requests, membership changes, history retrieval, reconnect, and blocked senders. Spam cannot starve economic commands. Keyboard/controller entry, focus return, mute, moderation, and reduced-motion pings work in the real game.

### MP-18 — Enforceable player trade and delivery contracts

Implement a bounded public/private offer system for direct goods/currency exchange and delivery contracts. Terms include parties, goods, quantities, source/destination, price/reward, deadline, escrow/reservations, partial-fill policy, cancellation, expiry, and default. Use transactional authoritative commands and actual inventory/logistics; do not teleport cargo or mint value. Integrate inspectable offer, accept/reject, fulfillment, and dispute/default outcomes in native UI. Public listings expose only intended terms.

Acceptance: two clients negotiate and fulfill a physical delivery; concurrent acceptances resolve once; expiry, insufficient goods, cancellation, disconnect, insolvency, and save/restart preserve conservation and obligations. AI may participate only through the same rules. Schema, editor, impact, migrations, and projection privacy ship with the feature.

### MP-19 — Joint ventures, agreements, and permissions

Implement a bounded joint-investment project using an existing supported asset/project class rather than inventing a new fleet/combat system. Define contributions, ownership shares, management permissions, proceeds/cost allocation, voting, withdrawal, dissolution, and insolvency. Add enforceable trade/access agreements and scoped embargo permissions where the game actually has authority to impose them. Political promises must either bind to implemented effects or be explicitly non-binding; do not expose fake functional controls for future League politics.

Acceptance: multiple houses fund and operate one venture, distribute proceeds deterministically, revoke management rights, and dissolve without duplicating property or money. Contract races, member loss, default, bans, save/reconnect, and team changes are tested. UI explains effects and permissions before commitment.

### MP-20 — Recovery finance and optional anti-grief rules

Implement bounded collateralized loans and restructuring that let a viable indebted house recover. Define lender/borrower consent, principal, interest/rounding, maturity, collateral locks, repayment priority, default, and asset transfer through normal economic rules. Add authored optional starting-market protections and bounded destructive-dumping rules with visible causes and remedies. Preserve legitimate competition; no hidden goods or money grants. Present recovery through contracts, assets, or refinancing before final defeat where rules permit.

Acceptance: indebted clients can recover through an actual funded agreement; default and restructuring preserve conservation and existing reservations. Test circular collateral, duplicate pledges, integer boundaries, quit/rejoin exploits, allied abuse, deadline/save boundaries, and final defeat. A protected and an unrestricted campaign demonstrate their advertised differences.

### MP-21 — Network and authority security hardening

Audit every shipping network entry point, RPC, snapshot, session operation, chat message, save/resume request, and admin command. Enforce size/count/decompression limits, numeric and stable-ID validation, authorization, nonce/epoch handling, queue budgets, rate limits, and credential redaction. Use the selected transport's authenticated security features correctly; do not invent cryptography. Privileged diagnostics remain opt-in and inaccessible to ordinary peers. Explain listen-host trust and dedicated-server advantages accurately.

Acceptance: malformed, replayed, reordered, oversized, compressed-bomb, unauthorized, and flooded inputs fail boundedly without mutation, leaks, crashes, or starvation of healthy peers. Test revoked credentials and admin privilege escalation. Shipping dependency/content scans exclude editor/generation workers, staging, test fixtures, and developer automation while retaining required runtime online libraries.

### MP-22 — Adverse-network, soak, and performance gates

Extend the multi-process runner to 2/4/8 clients plus AI and live simulation. Run zero-impairment and controlled 50/150/300 ms round-trip latency, jitter, 1/3/5% loss, brief outages, constrained bandwidth, joins, reconnects, and host/server recovery. Record the exact impairment setup; do not label loopback screenshots as internet evidence. Include an eight-hour soak with repeated joins, saves, trades, construction, and permission changes. Validate conservation and authorized projection convergence, not identical private digests across players.

Acceptance: meet the MP-01 budgets for server tick cost, memory, bandwidth per client, command feedback latency, snapshot size/time, and reconnect time on declared hardware/content. Report percentiles and worst cases. Severe-network cases may time out cleanly under the declared policy but must not corrupt authority. Profile before adopting Replication Graph/Iris or broad rewrites; retest measured fixes.

### MP-23 — Full player-flow UAT and authoring parity audit

Run the complete experience from clean local profiles through hosting/inviting, lobby changes, normal gameplay, late join, communication, contracts, cooperative/competitive results, disconnect/reconnect, save/resume, moderation, and recovery. Use ordinary packaged controls and real multi-machine internet play as well as semantic automation. Verify every gameplay inventory row from MP-01 again, including features added since the audit. Follow the GUI verification contract at all supported resolutions/scales and input methods.

Audit every multiplayer definition/property against editor metadata, schemas, migrations, validation, impact analysis, mock generation compatibility, and save/network visibility. Fix deficiencies in the same feature implementation; do not accept permanently missing editor support.

Acceptance: the matrix has correlated authoritative evidence and real captures for every user journey, including error paths and accessibility. No inaccessible action, placeholder screen, stale private state, test-only identity, or automation-only workaround satisfies a gate.

### MP-24 — Clean-build release acceptance and handoff

From a clean-checkout-equivalent state, build and package the client and dedicated server; run fast contracts, migrations, simulation regressions, multi-client suites, online-provider smoke tests, adverse-network/soak evidence review, UAT, save compatibility, and Shipping exclusion checks. Verify single-player still uses the same validated authority path and completes its existing acceptance journey. Reproduce deployment, fresh-account join, campaign backup/restore, and graceful shutdown using the shipped instructions.

Produce the final release report mapping each requirement to dated evidence and artifact paths, supported versions/platform/provider, measured limits, operator/user instructions, and remaining defects. Separate the finished multiplayer milestone from future full-campaign content. Missing real provider, dedicated binary, eight-player, privacy, continuity, or packaged UI evidence blocks full-release status. Do not declare completion by changing the original gate or hiding a failed test.

Acceptance: all mandatory rows pass on the release candidate. If any remain blocked, provide a concrete remaining-work list and keep the release status blocked.
