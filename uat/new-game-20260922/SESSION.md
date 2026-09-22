# Hansa New Game UAT — 2026-09-22

## Product profile

- Product: Hansa
- Type: Unreal Engine game
- Environment: local Development Editor
- Role: player
- Launch/build: `Scripts/Build.ps1 -Target HansaEditor -Configuration Development`
- Evidence: `Saved/BuildArtifacts/` automation logs
- Flow: start a production-profile New Game and advance the empty Lübeck session

## Evidence packet

### FLOW-NEWGAME-001 — Start New Game

Precondition: production definitions under `/Game/Hansa/Core` are discoverable.

Steps:

1. Initialize the production runtime profile.
2. Request New Game.
3. Confirm the new simulation starts at tick zero with no player buildings.
4. Advance one tick.
5. Start New Game again and compare the deterministic fingerprint.

Expected: the empty Lübeck session starts and remains playable.

Baseline observed: initialization rejected 208 discovered definitions because the
accepted catalog guard still expected 205 definitions (190 versus 187 after compound
exclusions).

## Issue ledger

| ID | Severity | Flow | Type | Summary | Acceptance / regression | Status |
|---|---|---|---|---|---|---|
| HNG-001 | P1 | FLOW-NEWGAME-001 | defect | Stale v34 catalog pin blocks every New Game | Catalog v35 reload and `Hansa.Integration.Save.FrontendNewGameAndRejectedRestore` pass through the production initializer | verified |

## Verification

- Result: pass
- Build: `Saved/BuildArtifacts/20260922-132504968-build-HansaEditor-Win64-Development`
- Catalog evidence: `Saved/BuildArtifacts/20260922-132524475-automation-Hansa.Integration.Authoring.EconomicAssetReload`
- Headless flow: `Saved/BuildArtifacts/20260922-132553795-automation-Hansa.Integration.Save.FrontendNewGameAndRejectedRestore`
- Native rendered flow: `Saved/BuildArtifacts/20260922-133036823-gui-repair-1280-720`

The native client exercised 25 title/settings/New Game/briefing/pause/save/load and
confirmation stages at 1280 × 720. New Game entered the production briefing and the
full journey completed successfully.
