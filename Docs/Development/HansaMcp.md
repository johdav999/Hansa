# Hansa MCP sidecar

`Tools/HansaMcp` is the external development-only adapter between Codex and the running non-Shipping Hansa process. It uses MCP over STDIO toward Codex and framed Windows named-pipe messages toward `HansaAutomation`. It has no Unreal headers, npm dependencies, provider SDKs or provider credentials.

Codex supports local STDIO MCP servers launched by a command and allows their command, arguments, working directory and forwarded environment variables to be configured in `config.toml`. See the official [Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp).

## Startup

Use a fresh pipe name and short-lived token for each game run. Both values must be present in the environment inherited by the game and sidecar; the token must be 16–128 non-whitespace characters.

```powershell
$env:HANSA_AUTOMATION_PIPE = "hansa-$PID-dev"
$env:HANSA_AUTOMATION_TOKEN = "replace-with-a-fresh-random-token"
& 'H:\Unreal\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  (Resolve-Path .\Hansa.uproject) `
  -game -HansaAutomation -HansaAutomationPermission=FixtureControl
```

For a project-scoped trusted Codex configuration, adapt the absolute path:

```toml
[mcp_servers.hansa]
command = "node"
args = ["C:/absolute/path/to/Hansa/Tools/HansaMcp/src/index.js"]
cwd = "C:/absolute/path/to/Hansa/Tools/HansaMcp"
env_vars = ["HANSA_AUTOMATION_PIPE", "HANSA_AUTOMATION_TOKEN"]
startup_timeout_sec = 10
tool_timeout_sec = 35
```

Restart the Codex client after changing MCP configuration. The sidecar may start before the game; each tool call makes up to eight connection attempts with capped exponential backoff. A missing or invalid pipe/token becomes a structured tool error rather than arbitrary console output.

Expected tool order:

1. `ping` may check transport liveness without a session.
2. `capabilities_get` discovers Unreal protocol `1.0`, permission ceiling and available capabilities.
3. `session_start` forwards the inherited token and requests exact capabilities.
4. Production sessions request `gameplay.query`, `gameplay.command`, and `fixture.control` with `FixtureControl` permission.
5. `fixture_list` then exact `fixture_load` initializes `mvp_production_chains_v1`, `lubeck_grain_shortage_v1`, the native semantic placement surface `empty_lubeck_build_v1`, or `integrated_lubeck_city_v1`.
6. `gameplay_query`, `gameplay_command`, `gameplay_assert`, `simulation_step`, `simulation_run`, and `simulation_run_until` inspect, control, assert, or advance bounded fixture state.
7. Semantic UI tools remain available under their separate capabilities.
8. `session_get`, `health`, and `session_stop` manage the sidecar-owned session identity.

## Wire contract

Each game-side frame is a four-byte unsigned little-endian payload length followed by one UTF-8 JSON object. Payloads are limited to 64 KiB. Requests and responses are correlated by `requestId`; one request is in flight at a time. The checked-in schema is `Tools/HansaMcp/schemas/automation-wire.schema.json`.

The wire operation allowlist is:

- `ping`
- `capabilities_get`
- `session_start`
- `session_get`
- `session_stop`
- `health`
- `fixture_list`
- `fixture_load`
- `gameplay_query`
- `gameplay_command`
- `gameplay_assert`
- `simulation_step`
- `simulation_run`
- `simulation_run_until`
- `semantic_find`
- `semantic_state`
- `semantic_activate`
- `semantic_focus`
- `wait_for`
- `screenshot_capture`

Within `gameplay_query`, S06-P03 adds `city.population` and `population.cohort`. The first returns city totals, typed trend, tier workforce, satisfaction, market access and staple reserve; the second returns one cohort's identities, capacity, workforce, factors and per-need consumption evidence. `gameplay_command` accepts the guarded `residence.upgrade` command. The game remains authoritative for every result and progression check; the sidecar only validates and forwards the bounded contract.

Failures preserve `code`, `correlationId`, safe `message`, `remedy` and `retryable`. MCP tool failures use `isError: true` plus the same structured object so Codex can correct the call. Malformed MCP methods remain JSON-RPC protocol errors.

## Shutdown and reconnect

Call `session_stop` before closing Codex or the game when practical. EOF, `SIGINT` or `SIGTERM` closes the sidecar pipe. Unreal unregisters its enabled-only ticker and closes the pipe during module shutdown.

After a disconnect, the sidecar drops the socket and reconnects on the next tool call. It does not replay an in-flight request. Retrying `ping`, `capabilities_get`, `session_get` or `health` is safe after reconnection when their prerequisites are still known. Do not blindly retry `session_start` or `session_stop` after an ambiguous disconnect; reconcile the session or restart the development game. A sidecar crash after session open currently leaves the game's session active until game shutdown.

## Log redaction

STDOUT is reserved for MCP JSON-RPC and never receives logs. Structured operational logs go to STDERR. Keys containing token, secret, password, authorization or credential are replaced with `[REDACTED]`; the known session token is also replaced wherever it appears in a string. The game logs neither token nor pipe name, and structured errors never echo authentication input.

## Contract tests

```powershell
pwsh -NoProfile -File Scripts\RunHansaMcpTests.ps1
```

The fake endpoint runs through the same frame encoder/decoder and covers session lifecycle, both fixture list/load paths, production, causal market, city-population and cohort-needs queries, guarded residence progression, shortage waits, controlled production recovery, read-only assertions, bounded step/run/run-until, semantic inspection/actions, observable waits, both screenshot sizes, ping, health, authentication rejection and missing capability. A real local named-pipe test delays endpoint startup to prove bounded reconnect. No game process, network access or live provider call is used.

The S05-P04 placement flow is available as `npm --prefix Tools/HansaMcp run smoke:placement`. It loads `empty_lubeck_build_v1`, commits a road through normal semantic input, proves the Warehouse `RoadRequired` failure, moves to the road-adjacent target, confirms the authoritative entity, waits on its semantic state, and captures native 1280×720 and 1920×1080 evidence.

The S06-P04 integrated flow is available as `npm --prefix Tools/HansaMcp run smoke:integrated`. It runs observable construction, delivery, production, bread-consumption, and population-growth checkpoints on `integrated_lubeck_city_v1`, reasserts their sticky completion, queries the combined city summary and cumulative consumption evidence, and captures both native evidence sizes under `S06P04`. See [IntegratedLubeckFixture.md](IntegratedLubeckFixture.md).

After starting an explicitly enabled development game as shown above, the optional real endpoint smoke is:

```powershell
npm --prefix Tools/HansaMcp run smoke:live
```

It requires a game launched with `-HansaAutomationPermission=ControlledActions` and performs ping, capability discovery, authenticated session start, semantic activation/focus, matched observable waits, both native screenshot captures, health and session stop, then prints one bounded JSON summary. It is intentionally not part of normal CI.

For a manual MCP STDIO smoke test without Unreal, run `node Tools/HansaMcp/src/index.js --fake` and send one compact JSON-RPC object per line. `--fake` is a test-only process mode and is never part of Shipping.
