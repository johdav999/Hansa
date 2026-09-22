# Firewood implementation and review status

Runtime implementation and a reload-verified development catalog are available. Generated presentation is imported into Unreal staging. **Normal New Game and Shipping still use accepted catalog v24; production promotion and final game binding have not happened.** See [FirewoodReview.md](FirewoodReview.md) for the exact asset/catalog approval packet.

## Implemented behavior

- `Good.Firewood`, `Recipe.SplitFirewood`, `Building.WoodcutterYard`, and `Need.Heating` form the fifth chain. Timber feeds planks, barrels and firewood. The yard uses two laborers and no artisans or tree-access requirement.
- Bakery, malt-house and brewery recipes use ordinary multi-input fuel reservations. No firewood was added to fishing, milling, sawmilling or cooperage.
- Both population tiers include resident-based heating. The authored 90-day seasonal calendar is deterministic, with summer/autumn/winter/spring multipliers 0/40/100/40 percent and an optional fixed season. Fractional milliunits are distributed deterministically; summer heating is omitted from satisfaction normalization.
- Existing sustained population evaluation handles unmet heating. Heating weight is 4,000; laborers can grow without beer when other relevant needs and the existing food-reserve gate are met. Empty-house founding retains the existing basic-service bootstrap rule and does not consume fuel before residents exist.
- Household protection is a derived floor in the existing physical ledger. It protects accessible city consumption pools, not warehouse stock, workshop buffers or incoming cargo. Existing batch/cargo commitments remain valid; new industrial reservations, withdrawals, deliveries and route loads use surplus above the floor. Household consumption may spend protected stock. No duplicate reserve inventory is created.
- Reserve policy is 3 days by default, adjustable 0–90 days. Explicit release persists until restored. Changes use an owned completed market and normal authoritative commands; unauthorized/out-of-range requests are atomic rejections. Multiple-owner market ambiguity is rejected.
- Market inspector actions have stable semantic IDs `Inspector.Heating.Increase`, `.Decrease`, `.Override`. Existing native widgets display protection/release and causal workshop blockers. Residence heating reads N/A in summer. City overview fields distinguish household demand, nominal enabled-workshop demand, committed stock, protected target, surplus and the winter target.
- Need authoring metadata/schema, validation, deterministic compilation, command fingerprint/codec, state hashing and save persistence are implemented. Save format 8 adds the named `Hansa.Save.7To8.DefaultHouseholdHeatingPolicy` migration; catalog compatibility checks remain strict.

## Code and content entry points

| Area | Paths |
| --- | --- |
| Calendar and reserve calculation | `Source/HansaSimulation/{Public,Private}/Population/HansaSeasonalNeeds.*`, `HansaHeating.*` |
| Ledger and outbound enforcement | `Inventory/HansaInventory.*`, `Logistics/HansaLocalLogisticsInternal.cpp`, `Trade/HansaTradeInternal.cpp` |
| Population and workshops | `Population/HansaPopulation.cpp`, `Production/HansaProduction.*` |
| Command/save/query contracts | `Commands/HansaGameplayCommand.*`, `Systems/HansaSimulationPipeline.cpp`, `Save/HansaSaveEnvelope.*`, `HansaSaveFields.inl`, `Queries/HansaSimulationReadOnly.*` |
| Runtime and UI | `Source/Hansa/Private/World/HansaRuntimeSimulationHost.cpp`, `HansaLubeckScenarioInitializer.cpp`, UI inspector/city overview/market/build-menu models and glyph mapping |
| Authoring and draft catalog | `Source/HansaEditor/Private/Definitions/HansaFirewoodCommandlet.cpp`, `HansaPopulationDefinitions.*`, economic compiler, `Docs/Development/FirewoodCandidate*.json` |
| Tests | `Source/HansaTests/Private/Simulation/HansaFirewoodTests.cpp`, `Source/HansaEditor/Private/Tests/HansaFirewoodCandidateTests.cpp`, `HansaFirewoodBalanceTests.cpp`, `HansaFirewoodWorkshopTests.cpp` |
| Model and icons | `SourceArt/Generated/Buildings/HansaWoodcutterYard_20260916/`, `SourceArt/UI/Firewood/`, `/Game/Hansa/Generated/Staging/FirewoodModel/` |

The accepted baseline is hash `C1BDF313543BF44A`; the disk-reloaded draft is `8BB8ACD607E70FDB` with 103 definitions: four additions, nine revisions, ninety unchanged hashes. The first transient draft hash differed because the four new display texts acquired serialized text identity. A fresh-process `-run=HansaFirewood -VerifyStaged` now verifies loaded hashes and reversed discovery order. Only the non-Shipping `-FirewoodCandidate` loader uses the candidate pin. Its yard still uses an explicitly labeled economy-test primitive until the reviewed mesh is promoted.

## UI component inventory

Existing native screen shell, navigation, panels, need/stock rows, charts and causal cards retain the approved palette, typography, layout and interaction styles. New content consists of the reserve increase/decrease/release actions, heating data/N/A states and two independently generated icons. Shared controls retain default, hover, pressed, disabled, selected/override and keyboard/controller focus states; status is expressed in text as well as visual state. No full-screen raster UI is shipped.

The firewood and yard masters are native 1254 × 1254 RGBA, built-in ImageGen generation, with exact sibling `.prompt.md` records. Display variants cover 16,20,24,28,32,40,48,56,64,80,96,112,160 px under the approved proportional GUI exception. Actual-size inspection is recorded; yard detail is subdued below 24 px. They are staged under `SourceArt/UI/Firewood/review-variants`, not copied into shipping icon paths. Native assembled-game screenshots across reference sizes are still pending.

## Actual verification

| Check | Result / evidence |
| --- | --- |
| Development Editor build | Passed; `Saved/GenerationJobs/Firewood_20260916/build-final-balance.log` |
| New integration tests | 5/5 passed across the focused runs: catalog/reload/policy/save and normal construction; seasonal schema/validation; full-year operating balance; semantic player reserve controls; authored workshop fuel accounting |
| New simulation tests | 2/2 passed: calendar/fractional demand and conserved protection/previous reservations/household withdrawals/explicit release |
| Workshop accounting | All three authored heat-consuming recipes checked every tick with zero or two fuel batches; pause/resume preserves progress and stock, finite fuel bounds cycles, missing-fuel blocker identifies firewood, and input/output quantities match completed batches. Focused test passed in `tests/20260916-210636243-automation-Hansa.Integration.Firewood.WorkshopFuelAccounting`; fresh Editor build also passed. This isolated full-staff fixture does not prove competing-workshop logistics or demolition refunds. |
| Ordinary construction | Yard placed through the construction gateway, completed with real delivery/workforce, and completed four firewood cycles in the bounded 800-tick fixture; that fixture has baseline development grants and is not balance proof |
| Full seasonal balance | All 360 daily checkpoints retained 50 residents; seven-day winter yard outage recovered; winter opening 186.65 kg fuel, year end 240 kg and 79 yard cycles. See `FirewoodBalance.md` and `.csv` |
| Residence variants | Every authored residence definition resolves a tier with exactly one heating need; this checks data inheritance, not every live upgrade interaction |
| Existing simulation suite | 83/85 passed. Rival shortage recovery and trade-network victory failures also exist in the 2026-09-11 baseline log; retained without unrelated fixes |
| Save suite | 12/13 passed after migration-count updates. The stale research-completion fixture is rejected for mismatched catalog content; no compatibility bypass was added |
| Existing authoring suite | 4/5 passed. Foundation schema golden differs by an unrelated empty `description` field; focused seasonal metadata/validation passes |
| Shipping code exclusion | Fresh Shipping build and audit passed; `release/20260916-204559581-shipping-exclusion-Win64/result.json` |
| Media reference audit | Production-reference audit run; result under `release/20260916-205404111-media-shipping-audit/result.json`. This is not proof of a freshly cooked content package |
| Model | Four Blender revisions inspected, packed master, clean FBX/GLB reimports, twelve turntable views, six assigned Unreal materials, saved/reopened stage and native hero/close/LOD captures |
| LOD and collision | Four saved Unreal LODs; one simple exterior collision box. Live gameplay collision and frame-time budgets are not yet accepted |

Commands and logs are retained under `Saved/GenerationJobs/Firewood_20260916`. Source-art prompts/provenance and final CSV/manifest copies are kept in repository documentation/source-art folders. Unrelated working-tree edits are preserved.

## Remaining gates and limitations

1. Explicit generated-asset/catalog promotion approval is required by `firewood.md`. Automatic approval review rejected direct production import; staging succeeded. The exact proposed destination and asset set are in `FirewoodReview.md`.
2. After promotion: bind the real yard mesh and icons, update canonical accepted seed/assets/catalog manifest/hash together, and verify ordinary New Game, actual model placement/ghosts, native UI, collision and fresh cooked-content exclusion. No primitive or unapproved staging reference is claimed as finished game presentation.
3. The measured balance result applies to the documented compact operating layout. Paid construction from a modest no-grant opening, brewery-drain stress, every trade/merchant path under a live reserve shortage, and complete compound-upgrade replay need additional end-to-end coverage. Generic ledger/route enforcement and data-tier checks are implemented, but those checks must not be misrepresented as all twelve prompt scenarios passing.
4. Full editor impact-analysis/generation UX acceptance is not established by metadata and compilation tests alone. Existing generic good/recipe/tier references carry the new dependency; dedicated designer-facing impact review remains a gate.
5. Model close-up bark/endgrain and fine iron/stone detail are approximations. The staged appearance is inspected; production game acceptance remains separate.

This is a verified review candidate with a concrete approval boundary, not a completed production rollout of every `firewood.md` gate.
