# ADR-0001 — Initial module and process boundaries

- Status: Accepted
- Date: 2026-09-01
- Decision owners: Hansa project

## Context

The audited project has one `Hansa` runtime module. The MVP requires a headless deterministic simulation, Unreal presentation/networking, editor authoring, development-only automation/tests, an MCP sidecar, and external AI/media generation. Placing these in one module would allow Engine presentation, editor APIs, provider SDKs, credentials, and test code to leak into authoritative simulation or Shipping.

## Decision

Use a modular monolith with these initial Unreal modules:

| Module | Host type | Responsibility | Allowed project dependencies |
| --- | --- | --- | --- |
| `HansaSimulation` | `Runtime` | Domain primitives, immutable definition registry, authoritative state, commands/events, systems, read-only queries and serialization primitives | None; `Core` engine module only where practical |
| `Hansa` | `Runtime` | Gameplay Framework, composition, Actors, world presentation, networking, input and C++ UI presentation models | `HansaSimulation` |
| `HansaEditor` | `Editor` | Authoring Studio, schema/validation/migration, editor graph/details UI, staging/promotion and worker client | `Hansa`, `HansaSimulation` |
| `HansaAutomation` | `DeveloperTool` | Fixtures, semantic UI adapter, read-only query adapters, controlled command submission, waits/assertions, screenshots and local endpoint | `Hansa`, `HansaSimulation` |
| `HansaTests` | `DeveloperTool` or a test-only target/module as proven in UE 5.8 | Domain, integration, UI, protocol and multiplayer tests | `HansaSimulation`; selected `Hansa` and `HansaAutomation` APIs |

Use two external development processes:

- `Tools/HansaMcp` speaks MCP to approved clients and a versioned local protocol to `HansaAutomation`.
- `Tools/HansaGenerationWorker` owns provider authentication, OpenAI/Tripo/ElevenLabs/TRELLIS adapters, downloads, conversion and job persistence; it speaks a versioned local protocol to `HansaEditor`.

Dependency direction is one-way:

```text
HansaEditor ───────► Hansa ───────► HansaSimulation
                         ▲                  ▲
HansaAutomation ─────────┴──────────────────┤
       ▲                                    │
HansaTests ─────────────────────────────────┘

Tools/HansaMcp ◄──── protocol ────► HansaAutomation
Tools/HansaGenerationWorker ◄─────► HansaEditor
```

Additional rules:

- `HansaSimulation` contains no Actor, UObject ownership, UMG/Slate, rendering, navigation, online, editor, automation, or provider dependency. Unreal reflection is allowed only if a specific serializable/value-type need is documented and does not pull a presentation dependency.
- `Hansa` never depends on editor, automation, tests, MCP, generation worker, or provider integration.
- `HansaEditor`, `HansaAutomation`, and `HansaTests` consume narrow public runtime seams; runtime modules do not use reverse callbacks into them.
- Start with project modules. A later ADR may move `HansaAutomation` to a project plugin only for reuse/distribution or independent plugin-UI enablement.
- Do not create a generic `HansaCore` dumping-ground module.

## Consequences

Positive:

- Headless simulation and dedicated-server work do not load presentation/editor/provider code.
- Shipping composition can exclude development capabilities by construction.
- UI, AI, network and automation share the same queries and command gateway.
- External provider failures and credential handling remain outside Unreal runtime processes.

Costs:

- Public interfaces and module build rules require deliberate ownership.
- Some integration tests need a development-only module or target.
- Cross-module refactoring is slightly more work than adding code to `Hansa`.

## Compliance

`S00-P02` must add executable checks that fail for forbidden project-module dependencies and must compile Development Editor, Development game and Shipping game targets. The `.uproject`, target rules, `.Build.cs` files and Shipping receipts are the evidence; a diagram alone is insufficient.

Implementation evidence, 2026-09-01: `S00-P02` selected `HansaTests` as a `DeveloperTool` project module, compiled all five modules in Development Editor, compiled the development game without `HansaEditor`, compiled Shipping with only the two runtime modules, and passed `Hansa.Architecture.Modules.Loadability` plus `Hansa.Architecture.Modules.Boundaries`.

## Deferred

- Exact dedicated-server target composition.
- Whether a later dedicated test target adds value beyond the proven `DeveloperTool` `HansaTests` module.
- Automation transport/authentication details.
- Later extraction of AI, UI, online or developer modules based on measured size/build pressure.
