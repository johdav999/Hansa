# Enhanced MVP visible-content completeness — EMVP-P02

## Outcome

The checked-in manifest at
[`Tests/Golden/enhanced_mvp_visible_content_v1.json`](../../Tests/Golden/enhanced_mvp_visible_content_v1.json)
is the machine-readable inventory for everything that can be visible in the enhanced golden session.
It is deliberately separate from gameplay identity and provider provenance: stable Hansa definition and
presentation-role IDs own the work, while file paths describe the current and intended implementation.

The 2026-09-07 audit contains 124 entries. Of these, 121 are golden-path roles and three are explicit
`not-shown` exclusions: Smithy, Brewery, and individual citizen Actors. The latter prevents “citizens
if shown” from becoming an accidental character-production requirement; population remains cohort data
presented through residences and native UI in this slice.

## Coverage

The manifest covers every required category:

- world/city composition, buildings, construction stages, road preview and final variants;
- the Cog, local cargo wagon, and explicit citizen-presentation policy;
- harbor equipment, production-state props, cargo props, vegetation, street dressing, land, water,
  shore, harbor wet/dry and road/terrain transitions;
- placement, road, construction, production, population, market, route, research, alert and UI effects;
- all ten good icons plus building, research, status and control icon families;
- world cursors, placement/service/production/route/selection/tooltip overlays;
- frontend, root HUD, construction, placement, road, inspector, City Overview, Market, trade route,
  Research, Scenario, onboarding, results, pause, confirmation, demolition and save dialogs;
- reusable empty, loading and error treatments;
- native 1280×720 and 1920×1080 capture contracts with raster resampling prohibited.

Every entry records the required EMVP-P02 fields: stable ID, owning definition, presentation role,
current reference, explicit status, intended canonical path, delivery prompt, LOD, collision, pivot,
footprint/layout requirements, proof needed, and UI state coverage where applicable.

## Audited status

| Status | Count | Meaning |
| --- | ---: | --- |
| `production-ready` | 3 | Bakery, animated Mill, and its rotor have accepted production references. |
| `unverified-production` | 1 | The laborer-residence production path exists, but the enhanced proof set is incomplete. |
| `prototype-native` | 20 | Functional native world/UI code exists but is not final production presentation. |
| `engine-placeholder` | 25 | The visible path uses an Engine basic shape/material and is release-blocking. |
| `staging-only` | 5 | Existing DirtRoad variants remain under generated staging and are release-blocking. |
| `missing` | 67 | No current accepted presentation exists. |
| `not-shown` | 3 | Deliberately excluded from the enhanced golden session. |

The counts are an audit snapshot, not a progress target. Later prompts update the same entries rather
than deleting gaps, weakening `goldenPath`, changing stable IDs, or moving work into a prose backlog.
An entry becomes `production-ready` or `native-production` only after its listed proof exists.

## Definition binding

The validator resolves the reviewed economic seed catalog and compares every bound manifest entry with
the exact definition property:

- the twelve golden building presentations use `PresentationMeshOrActor`;
- `Vehicle.Cog` and `Vehicle.Wagon` use `PresentationMesh`;
- all ten `Good.*` entries use `Icon`.

This prevents the manifest from claiming a promoted asset while the runtime definition still resolves a
cube, a staging path, or nothing. Smithy and Brewery are also bound for drift detection, but their
`not-shown` policy is enforced by construction-catalog negative evidence rather than release art.

## Automated gates

`Hansa.Content.VisibleManifest.Completeness` is the wave-zero gate. It must remain green while work is
in progress. It fails on malformed entries, unknown statuses, duplicate stable IDs, missing categories,
missing required screens/resolutions, a missing definition binding, or drift between the manifest and
the reviewed definition catalog. It also proves that strict validation recognizes all four mandated
release failures:

- `HVCM-DEF-001`: a golden definition has no presentation;
- `HVCM-PATH-001`: a golden reference points to generated staging or Developer content;
- `HVCM-PATH-002`: a golden reference uses an Engine basic-shape fallback;
- `HVCM-UI-001`: a required UI state is absent.

`Hansa.Release.VisibleManifest.Readiness` is the release-facing gate. It is intentionally red at
EMVP-P02 because this prompt inventories later work rather than fabricating its completion. Each
diagnostic names the stable entry, owning delivery prompt, and intended canonical destination. The test
becomes green only when every golden entry is `production-ready` or `native-production`, definition
bindings agree, forbidden paths are gone, and every required state is implemented.

Run the gates with:

```powershell
pwsh -NoProfile -File Scripts/RunAutomationTests.ps1 `
  -TestFilter Hansa.Content.VisibleManifest.Completeness `
  -EngineRoot 'H:\Unreal\UE_5.8'

pwsh -NoProfile -File Scripts/RunAutomationTests.ps1 `
  -TestFilter Hansa.Release.VisibleManifest.Readiness `
  -EngineRoot 'H:\Unreal\UE_5.8'
```

The second command must fail until the later enhanced-MVP prompts have supplied and proven the
presentation set. Treating that expected failure as a waiver would defeat the gate.

## Updating the manifest

When a later prompt delivers an entry:

1. keep its stable ID, owning definition, role, requirements, and canonical destination stable unless
   the gameplay/design contract genuinely changes;
2. promote any generated artifact out of `/Generated/Staging/` and keep provider IDs out of gameplay
   identity;
3. update the owning definition through the existing editor/schema/version path where it binds an
   asset;
4. change `currentReference`, `status`, and implemented UI states only after the real implementation
   exists;
5. attach the proof listed by the entry, including real assembled viewport captures for visual rows;
6. run both gates and the prompt-specific tests.

Do not satisfy readiness by pointing to a source-art render, full-screen generated mockup, isolated
Slate proof surface, Engine primitive, Developer asset, or unverified staging import.
