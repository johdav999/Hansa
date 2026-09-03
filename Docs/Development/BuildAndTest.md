# Hansa — Build, Test and CI Entry Points

## 1. Purpose

The scripts under `Scripts/` are the supported local and future-CI entry points for the current Windows/UE 5.8 baseline. They discover Unreal from an explicit argument, the `HANSA_UNREAL_ENGINE_ROOT` environment variable, Windows registrations, or the Epic Launcher manifest. They do not assume that Unreal is installed on `C:`.

Generated project files, binaries, Unreal logs, automation output and script evidence remain under ignored directories. Script evidence defaults to `Saved/BuildArtifacts/<timestamp>-<operation>/` and contains the invoked command log plus a machine-readable `result.json`.

## 2. Engine selection

Choose one method:

```powershell
$env:HANSA_UNREAL_ENGINE_ROOT = 'H:\Unreal\UE_5.8'
```

or pass the root to any entry point:

```powershell
pwsh -File Scripts/Build.ps1 -EngineRoot 'H:\Unreal\UE_5.8'
```

If neither is supplied, the scripts read `EngineAssociation` from `Hansa.uproject` and inspect registered/Launcher installations. Every candidate must contain the required tools and its `Build.version` must match the project's engine association. Failure lists the candidates and tells the caller how to override discovery.

## 3. Commands

### Verify repository conventions and safety policy

```powershell
pwsh -File Scripts/VerifyRepositoryConventions.ps1
```

This engine-independent gate checks convention files, the persistent Visual Studio 2022 generator pin and any current generated project metadata, high-confidence credential patterns, provider leakage into runtime/configuration, forbidden production references to Developer/generated staging, safe config defaults, cook exclusions, ignored output, Git LFS routing, test names and the fixture template. It writes a machine-readable result under `Saved/BuildArtifacts/` and never prints suspected credential values.

### Generate IDE project files

```powershell
pwsh -File Scripts/GenerateProjectFiles.ps1
```

This is useful for local IDE setup. CI builds do not require a generated solution. The repository baseline is Visual Studio 2022, so the script explicitly passes Unreal's `-2022` generator option and verifies that the resulting solution uses Visual Studio version 17, MSBuild tools version 17.0, and platform toolset `v143`.

The source-controlled `Saved/UnrealBuildTool/BuildConfiguration.xml` also pins `VisualStudio2022`. This second layer is required because project files regenerated from Unreal Editor or Windows Explorer do not receive the script's command-line option. Do not remove or locally empty that file: when multiple Visual Studio major versions are installed, Unreal otherwise selects the newest IDE and can generate a VS 2026 project that Visual Studio 2022 reports as incompatible. A future IDE-major migration must update the persistent configuration, generator checks, `.vsconfig`, documentation, and convention audit together.

Launcher builds that do not contain `GenerateProjectFiles.bat` automatically use UnrealBuildTool's `-projectfiles` mode.

### Build Development Editor

```powershell
pwsh -File Scripts/Build.ps1 -Target HansaEditor -Configuration Development
```

### Build Development game

```powershell
pwsh -File Scripts/Build.ps1 -Target Hansa -Configuration Development
```

### Run headless Hansa automation tests

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa
```

The test entry point builds Development Editor unless `-SkipBuild` is supplied. It fails if the filter matches zero tests, Unreal returns a nonzero exit code, a test reports failure, the Unreal log is missing, or the successful completion marker is absent.

### Run HansaMcp contract tests

```powershell
pwsh -File Scripts/RunHansaMcpTests.ps1
```

This engine-independent gate requires Node.js 22 or newer and runs the dependency-free sidecar's framing, fake endpoint, MCP lifecycle, structured-error, log-redaction and delayed named-pipe reconnect tests. It launches no game and makes no network or provider call. Results are retained under the normal ignored build-artifact root.

### Build and audit Shipping exclusion

```powershell
pwsh -File Scripts/VerifyShippingExclusion.ps1
```

This builds `Hansa Win64 Shipping`, verifies module host types and runtime dependency direction, then scans the Shipping target receipt and executable for representative editor/automation/test/tool markers. It writes hashes and scan results to `result.json`.

This is a target-level gate. It does not yet replace the later packaged/cooked/depot audit required by ADR-0004.

### Run the current CI-equivalent sequence

```powershell
pwsh -File Scripts/InvokeCI.ps1 -TestFilter Hansa
```

The sequence first verifies repository conventions, runs HansaMcp contract tests, then builds Development Editor, runs headless Hansa tests, builds the Development game, builds Shipping, and performs the exclusion audit. Add `-GenerateProjectFiles` only when a CI environment explicitly needs IDE files.

Use a different artifact root when a CI runner requires it:

```powershell
pwsh -File Scripts/InvokeCI.ps1 -ArtifactsRoot 'D:\CIArtifacts\Hansa'
```

## 4. Exit and evidence behavior

- Success returns process exit code `0`.
- A repository policy violation, missing engine/tool, invalid argument, failed native command, zero-test filter, failed test, missing artifact, forbidden dependency or forbidden Shipping marker terminates with a nonzero exit code and an actionable message.
- Every native invocation records its resolved executable, arguments, working directory and combined output.
- The default artifact and normal Unreal log directories are ignored by Git.
- Scripts never delete old evidence; retention/cleanup is the CI host's responsibility.
- No provider credentials or live provider calls are part of these commands.

## 5. Platform-neutral CI checklist

No CI provider is selected in the repository, so `S00-P03` deliberately adds no GitHub Actions, Azure Pipelines, Jenkins or other provider-specific configuration.

A future Windows runner must:

1. Check out the repository without discarding Git LFS pointers, then run `git lfs pull`.
2. Provide the approved UE 5.8 build and compatible Visual Studio 2022/MSVC plus Windows SDK.
3. Install Node.js 22 or newer for the dependency-free HansaMcp contract tests.
4. Set `HANSA_UNREAL_ENGINE_ROOT` or pass `-EngineRoot`.
5. Run `pwsh -File Scripts/InvokeCI.ps1 -TestFilter Hansa`.
6. Treat the PowerShell process exit code as authoritative.
7. Upload `Saved/BuildArtifacts/**`, `Saved/Logs/**` and relevant `Saved/Automation/**` output on success and failure.
8. Keep normal CI free of OpenAI, Tripo, ElevenLabs, TRELLIS or other paid live calls.
9. Add packaged Shipping cook/stage inspection when packaging is introduced; do not treat the current target-level audit as that future proof.
10. Cache Derived Data Cache only with an engine/changelist/project-content-aware key and never commit the cache.
11. Keep all credentials in the CI secret store and out of command output, `.ini`, source, assets and uploaded public logs.

## 6. Adding a future entry point

New scripts should dot-source `Scripts/HansaBuild.Common.ps1`, use `Get-HansaBuildContext` when engine tools are required, create an ignored artifact directory, invoke native tools through `Invoke-HansaNativeCommand`, emit `result.json`, and throw on any incomplete or ambiguous success state. Do not copy engine installation paths into source-controlled configuration. Repository layout and policy changes must also update [RepositoryConventions.md](RepositoryConventions.md) and its executable guard when applicable.
