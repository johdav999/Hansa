# ADR-0004 — Shipping exclusion of development capabilities

- Status: Accepted
- Date: 2026-09-01
- Decision owners: Hansa project

## Context

The MVP needs semantic inspection, controlled commands, screenshots, MCP, editor authoring, AI/media provider jobs, fixtures and test evidence. Hiding these behind a menu or runtime boolean would still ship attack surface, provider configuration and unnecessary code/content. The released game must function without them and must not contain them.

## Decision

Use layered exclusion rather than a single switch:

### Build composition

- `HansaEditor` is an `Editor` module and is never a dependency of runtime targets.
- `HansaAutomation` and `HansaTests` are development/test-only modules. `HansaAutomation` uses host type `DeveloperTool`, is added only where UBT enables developer tools, and is explicitly omitted for Shipping as defense in depth.
- `Hansa` and `HansaSimulation` never depend on these modules. Any minimal compile-time integration seam is transport-neutral and guarded by `WITH_HANSA_AUTOMATION`, which is `0` for Shipping.
- `Tools/HansaMcp`, `Tools/HansaGenerationWorker` and provider SDKs are external developer tools and are not Unreal runtime dependencies.

### Startup and access

- Development/Test endpoints are disabled by default. They require an explicit approved launch flag and short-lived authorization material.
- When disabled, no pipe/socket opens, no commands register and no per-frame automation work occurs.
- Read-only and mutating capabilities are negotiated separately. Mutation uses the normal typed gameplay command gateway and authority checks.
- No interface exposes arbitrary console execution, arbitrary UObject/Blueprint reflection, memory, filesystem, SQL, C++ invocation, or client-supplied asset/class loading.

### Content and secrets

- QA fixtures, automation configuration, test credentials, generated staging, Developer preview content and development manifests are excluded from Shipping cook/stage/depot rules.
- Provider credentials exist only in the OS credential store or injected external-worker environment. They never enter source, `.ini`, Data Assets, Blueprint defaults, logs, manifests or packaged files.
- Shipping content cannot reference `/Generated/Staging/`, `/Developer/`, external provider URLs or transient job artifacts.

### Release evidence

A release gate must build and package Shipping, then inspect build receipts, staged manifests/content and representative binaries/config. It must prove that forbidden modules, tools, endpoints, flags/symbols, provider settings/credentials and QA-only content are absent. It must also launch Shipping and prove an automation enable flag is ignored/rejected and no endpoint opens.

## Consequences

Positive:

- Released builds have materially less attack surface and no provider credential path.
- Automation can evolve rapidly without becoming a runtime compatibility contract.
- Disabling development capability requires no Blueprint rewiring or content edits.

Costs:

- Runtime seams must remain useful without a direct automation dependency.
- Separate Development/Test and packaged Shipping verification is required in CI.
- Some end-to-end tests require multi-process orchestration outside the game.

## Compliance

`S00-P02` must prove module-level exclusion; `S00-P03` must provide repeatable build/test entry points; later automation/provider prompts must extend the artifact audit when they add new files or settings. A successful compile without development modules present is not sufficient proof—the gate becomes meaningful only after those modules exist in supported non-Shipping targets.

Implementation evidence, 2026-09-01: the development Editor/game targets compile `HansaAutomation` and `HansaTests`, while the Shipping game compiles only `HansaSimulation` and `Hansa`. Shipping runtime compilation asserts `WITH_HANSA_AUTOMATION == 0`; the Shipping receipt and executable contain no representative forbidden module/macro strings. Packaged/cooked/depot inspection remains a later gate.

`S00-P03` makes that target-level evidence repeatable through `Scripts/VerifyShippingExclusion.ps1` and includes it in the local/CI contract exposed by `Scripts/InvokeCI.ps1`. The audit records hashes and marker results under ignored `Saved/BuildArtifacts/` paths and fails closed when expected artifacts are missing. It still does not satisfy the later packaged/cooked/depot inspection or Shipping launch probe required for release.

`S00-P04` adds checked-in never-cook rules for `/Game/Hansa/Developer` and `/Game/Hansa/Generated/Staging`, ignores transient staging, disables the Android file server/network default and removes its static token, and runs `Scripts/VerifyRepositoryConventions.ps1` before the CI-equivalent build sequence. This establishes the source/configuration boundary; real asset referencer analysis and packaged-cook inspection remain later executable gates.

## Deferred

- Exact named-pipe/loopback authentication and token lifecycle.
- Final cook labels/directories for fixtures and Developer content.
- Binary-symbol inspection implementation and acceptable false-positive policy.
- Release depot/platform packaging rules.
