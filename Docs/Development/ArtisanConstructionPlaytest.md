# Temporary artisan construction playtest — 2026-09-16

The artisan definition intentionally has `bUpgradeOnly=true`. The construction presenter used that metadata to disable its card, preventing selection or drag placement. The simulation already supports constructing this residence through its ordinary placement command.

For the requested model playtest, `Config/DefaultEngine.ini` enables:

```ini
[Hansa.ConstructionTesting]
AllowDirectArtisanResidence=True
```

Set this to `False` (or remove the key) and restart the editor/game to restore upgrade-only construction. The override is ignored in Shipping. It applies only to `Building.Residence.Artisan`; technology locks, resource costs, road access, footprint and terrain validation still apply. Existing residence upgrades continue to work. Authored assets, schemas, catalog version/hash and save format are unchanged. Already constructed houses remain when the switch is disabled.

The existing native residence card, tooltip, focus/disabled states, generated icon and world-placement preview are reused; no artwork or layout change.

Regression `Hansa.UI.BuildMenu.ArtisanConstructionPlaytest` exercises semantic card activation, rejection without road access, ordinary road-adjacent construction through the command gateway, and restoring the upgrade-only lock. The existing compact-tray and catalog progression tests explicitly disable the temporary switch in memory and restore its prior value afterward.

## Verification

- Main Development Editor build: PASS (`Saved/Logs/ArtisanPlaytest-Build.log`).
- `ArtisanConstructionToggle`: PASS; off/on/off, other upgrade-only buildings and research requirements covered (`Saved/Logs/ArtisanPlaytest-ToggleTests.log`).
- `ArtisanConstructionPlaytest` and `CompactTrayAndControllerNavigation`: 2/2 PASS (`Saved/Logs/ArtisanPlaytest-Tests.log`).
- Initial gameplay verification encountered the concurrent labour-court catalog update; rerun passed after that work advanced to v23. This change does not alter the catalog.
- Behaviour-only change; no new visual asset or visual QA claim.
