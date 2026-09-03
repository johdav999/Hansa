# HansaAutomation session boundary

`S02-P02` provides the security and negotiation contract. `S02-P03` connects that service to the explicitly enabled Windows named-pipe endpoint described in [HansaMcp.md](HansaMcp.md). `S02-P04` adds bounded semantic UI, wait and native evidence adapters. `S03-P04` adds the named headless production-fixture adapter. MCP remains out of process.

## Startup policy

Ordinary Development and Editor processes remain disabled. Explicit enablement requires all of:

```text
process environment: HANSA_AUTOMATION_TOKEN=<16-to-128-character-short-lived-token>
process environment: HANSA_AUTOMATION_PIPE=<safe-per-run-local-pipe-name>
command line: -HansaAutomation -HansaAutomationPermission=ReadOnly
```

`MaximumPermission=ReadOnly` is the checked-in configuration ceiling. `ControlledActions` or `FixtureControl` must be authorized at process startup through a deliberate local test profile; a client request cannot increase the ceiling. The launcher supplies the token only in the child process environment because Unreal records its command line. Tokens are compared during session open and are never included in snapshots, errors or logs.

If the enable flag/config is present but the token, pipe name, permission or endpoint startup is invalid, the module records a safe startup error and constructs a disabled service. `IsTransportRequested()` therefore records intent, while `IsSessionBoundaryEnabled()` records the fail-closed result. When disabled, no pipe or ticker exists.

## Protocol and sessions

The foundation protocol is `1.0`. Compatible clients use the same major and a minor no newer than the process. Capability discovery advertises:

- `session`
- `capabilities`
- `health`
- `gameplay.query`
- `gameplay.command`
- `fixture.control`
- `semantic-ui`
- `screenshots`
- `wait-assertions`

Gameplay queries require `ReadOnly`, simulation advancement requires `ControlledActions`, and fixture loading requires `FixtureControl`. Production operations remain exact-name, bounded and session-capability scoped.

A successful open returns a process-generated session ID, controller identity, negotiated version, permission, exact granted capabilities and monotonic open time. Only one controller/session is active. Close clears the session; it does not alter content or gameplay state.

## Request safety

Every request context has a 1–64 character correlation ID using alphanumeric, dot, underscore, colon or hyphen characters. The receiving endpoint captures monotonic enqueue time and a positive timeout of at most 30 seconds. Invalid and expired requests fail before session or operation authorization.

Session operations validate, in order:

1. process enablement and request context;
2. active session/controller identity;
3. allowlisted operation mapping;
4. explicitly granted capability;
5. negotiated permission.

Structured failures return code, correlation ID, safe message, remedy and retryability. Stable codes include `Disabled`, `InvalidCorrelationId`, `InvalidTimeout`, `TimedOut`, `IncompatibleProtocol`, `AuthenticationFailed`, `PermissionDenied`, `MissingCapability`, controller/session conflicts and `OperationUnsupported`.

## Deliberately absent surfaces

The public API accepts no console text, UObject/class/asset path, filename, arbitrary reflection request, raw state pointer, SQL, script or generic query expression. Production advancement maps typed requests to `FHansaGameplayCommandGateway`; permission does not authorize a direct setter.

## Verification

```powershell
pwsh -NoProfile -File Scripts\Build.ps1 -Target HansaEditor -Platform Win64 -Configuration Development
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Architecture.Automation -SkipBuild
pwsh -NoProfile -File Scripts\VerifyShippingExclusion.ps1
```

The focused filter covers seven lifecycle, negotiation, rejection, timeout and Shipping-source tests. The artifact-level Shipping audit remains the executable proof that the module and representative protocol markers do not enter the target.

The external contract suite in `Scripts/RunHansaMcpTests.ps1` additionally proves framing, MCP lifecycle, fake endpoint behavior, redaction and reconnect without launching Unreal.
