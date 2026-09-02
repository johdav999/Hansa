# Hansa

Hansa is an Unreal Engine 5.8 trade and city-builder game.

## Requirements

- Unreal Engine 5.8
- Visual Studio 2022 with the **Game development with C++** workload
- PowerShell 7 (`pwsh`)
- Git LFS

## Getting started

```powershell
git lfs install
git clone https://github.com/johdav999/Hansa.git
cd Hansa
git lfs pull
```

Generate IDE project files, then open `Hansa.uproject` in Unreal Editor:

```powershell
pwsh -File Scripts/GenerateProjectFiles.ps1
```

Project generation is pinned to Visual Studio 2022 by default even when a newer Visual Studio is installed.

Generated Unreal directories and IDE files are intentionally excluded from version control. Unreal packages and common source-art/media formats are stored through Git LFS.

## Build and test

The scripts discover the UE 5.8 installation automatically where possible. You can override discovery for any command with `-EngineRoot` or set `HANSA_UNREAL_ENGINE_ROOT`.

```powershell
pwsh -File Scripts/VerifyRepositoryConventions.ps1
pwsh -File Scripts/Build.ps1 -Target HansaEditor -Configuration Development
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa
pwsh -File Scripts/VerifyShippingExclusion.ps1
```

Run the complete current CI-equivalent sequence with:

```powershell
pwsh -File Scripts/InvokeCI.ps1 -TestFilter Hansa
```

Logs and machine-readable results are preserved under the ignored `Saved/BuildArtifacts/` directory. See [Build, test and CI entry points](Docs/Development/BuildAndTest.md) for configuration, failure behavior and the platform-neutral CI checklist.

Before adding code, assets, fixtures, configuration, generated content, or tests, follow the canonical [repository conventions](Docs/Development/RepositoryConventions.md). Developer and generated-staging assets are excluded from cook, transient staging/evidence is ignored, and binary/media assets—including PNG references—use Git LFS.

## Design documentation

- [Game concept](Docs/GameConcept.md)
- [Technical architecture](Docs/TechnicalArchitecture.md)
- [Integrated MVP scope](Docs/MVP.md)
- [Integrated MVP sprint and prompt plan](Docs/MVPSprintPlan.md)
- [Current development baseline](Docs/Development/Baseline.md)
- [Build, test and CI entry points](Docs/Development/BuildAndTest.md)
- [Repository conventions](Docs/Development/RepositoryConventions.md)
- [Architecture decision records](Docs/Architecture/Decisions/README.md)
- [Editor and AI-assisted authoring architecture](Docs/EditorArchitecture.md)
- [Authoring editor MVP workstream](Docs/EditorMVP.md)
- [UI and GUI design brief](Docs/UIDesignBrief.md)
- [GUI and image asset workflow](Docs/UIAssetWorkflow.md)
