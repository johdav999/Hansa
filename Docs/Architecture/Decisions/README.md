# Hansa Architecture Decision Records

Architecture Decision Records (ADRs) capture durable technical decisions that implementation must follow. They complement [TechnicalArchitecture.md](../../TechnicalArchitecture.md): the architecture document explains the whole target system, while an ADR locks one concrete choice, its tradeoffs, and its compliance checks.

## Status vocabulary

- **Proposed:** under review; implementation must not assume acceptance.
- **Accepted:** active decision for new work.
- **Superseded:** replaced by a later ADR; retained for history.
- **Deprecated:** still present but must not be used for new work.

## Index

| ADR | Status | Decision |
| --- | --- | --- |
| [ADR-0001](0001-module-and-process-boundaries.md) | Accepted | Initial Unreal module and external-process boundaries |
| [ADR-0002](0002-stable-identifiers.md) | Accepted | Stable definition and runtime entity identifiers |
| [ADR-0003](0003-deterministic-units-and-time.md) | Accepted | Fixed-point domain units, simulation time and deterministic ordering |
| [ADR-0004](0004-shipping-exclusion.md) | Accepted | Compile-time and packaging exclusion of editor, automation, test and generation tooling |
| [ADR-0005](0005-command-gateway-and-domain-events.md) | Accepted | Single typed transactional command gateway and ordered domain-event publication |
| [ADR-0006](0006-normalized-state-hashes-and-diagnostics.md) | Accepted | Versioned normalized subsystem hashes and first-divergence diagnostics |
| [ADR-0007](0007-reflected-definition-schema-contract.md) | Accepted | Reflected definition metadata as the generic editor and JSON Schema contract |
| [ADR-0008](0008-automation-session-and-capability-boundary.md) | Accepted | Opt-in authenticated automation sessions with negotiated capabilities and permissions |
| [ADR-0009](0009-mcp-sidecar-and-framed-named-pipe.md) | Accepted | External MCP STDIO sidecar and bounded Windows named-pipe framing |
| [ADR-0010](0010-semantic-ui-waits-and-native-evidence.md) | Accepted | Widget-neutral semantic UI, observable waits and native screenshot evidence |

## ADR rules

1. Use the next four-digit number and a short kebab-case filename.
2. Record context, decision, consequences, compliance evidence and deferred choices.
3. Do not edit an accepted ADR to reverse its decision. Add a superseding ADR and link both records.
4. Clarifications that do not change the decision may be added with a dated amendment.
5. Code review must identify the ADR when changing a locked boundary or invariant.
6. An accepted decision without an executable check remains implementation debt and must be listed as such in the development baseline.

## Initial decision dependency

```mermaid
flowchart LR
    A1[ADR-0001\nmodule boundaries] --> A4[ADR-0004\nShipping exclusion]
    A2[ADR-0002\nstable identifiers] --> A3[ADR-0003\ndeterministic state and time]
    A2 --> Save[future save/protocol ADRs]
    A3 --> Save
	A2 --> A5[ADR-0005\ncommand gateway and events]
	A3 --> A5
	A3 --> A6[ADR-0006\nstate hashes and diagnostics]
	A5 --> A6
	A1 --> A7[ADR-0007\nreflected definition schemas]
	A2 --> A7
    A1 --> A8[ADR-0008\nautomation sessions]
    A4 --> A8
	A5 --> A8
	A8 --> A9[ADR-0009\nMCP sidecar and pipe framing]
	A9 --> A10[ADR-0010\nsemantic UI, waits and evidence]
```

The audited implementation status for these decisions is maintained in [Development/Baseline.md](../../Development/Baseline.md).
