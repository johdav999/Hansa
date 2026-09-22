# Artisan production implementation
Status: playable review candidate verified. Default-catalog promotion awaits explicit generated-content approval.

## Scope
Tools: iron bars + charcoal -> Smithy -> tools.
Leather: raw hides + tanning bark -> Tannery -> leather.
Shoes: leather -> Shoemaker -> shoes.
Supporting charcoal: timber -> Charcoal Burner -> charcoal.

## Construction classification
Additive ConstructionTier enum: Legacy, DayLaborers, Craftsmen, Merchants.
Legacy defaults preserve existing content hashes and whole-chain classification.
Explicit cards belong only to their selected tier; selectors aggregate card visibility.
Charcoal burner is DayLaborers. Smithy, Tannery and Shoemaker are Craftsmen.
Input supply and workforce do not determine the authored construction tier.

## Component inventory
| Component | Implementation | States |
|---|---|---|
| HUD shell and category/tier navigation | Existing native Slate and approved art | existing default, selected, focus, hover, pressed |
| Tools and Shoes chain selectors | Existing native good selectors, individual raster good icons | default, selected, focus |
| Workshop cards | Existing native cards, individual raster building illustrations | default, hover, pressed, selected, disabled, focus |
| Recipe inputs and outputs | Native quantity/text with individual raster goods | supplied, shortage, unavailable |
| Production inspector | Existing native inspector/ledger/progress | working, paused, blocked, full storage |
| Placement overlay | Existing native footprint and authored 3D ghost | valid, warning, invalid |
| Feedback | Existing native causal text and semantic symbols | loading, empty, warning, error |
| Decorative imagery | Reuse existing frame/ornaments | no new decoration |

## Raster specifications
Reference: composed construction tray, landscape 1536x1024 requested, reference only.
Icons: one component per generation, square 1024x1024 master requested, transparent.
Building cards: 88 logical pixels; good glyphs 24/32 logical pixels.
Document high-quality proportional display variants under the approved GUI resizing exception.
Palette: Baltic Navy #152A35, Harbor Slate #29424D, Brass #C19A52, Chalk #FAF7EF,
Linen #F2E9D8. Naturally colored objects with restrained highlights.
Text and state surfaces remain native, with existing localization and focus behavior.
All generated assets require actual native and display-size inspection.

## Verification ledger
| ID | Flow | Status |
|---|---|---|
| AP-01 | Explicit tier prevents mixed workforce duplication | PASS: DebugGame build; three ConstructionProgression tests |
| AP-02 | Tools/leather/shoes production and input blockers | PASS: all four authoritative recipes, each missing input, workforce failure, exact consumption/output |
| AP-03 | Finished workshop models and actual Unreal placement | PASS: native controls build all four workshops; live mesh bindings asserted; three LODs and convex collision saved |
| AP-04 | Editor/schema/hash/save compatibility | PASS: reflected metadata, Legacy hash preservation, invalid enum rejection, recipe save round-trip and continuation; old catalog rejected explicitly |
| AP-05 | Real in-game GUI and controller journey | PASS: six tier/chain/accessibility states and four workshop inspections at 1280x720,1920x1080,2560x1440,3440x1440; large-text caption fix rechecked at1280 |

## Environment
Windows file helper currently fails with apply deny-read ACLs.
Shell reads/writes require escalated execution. Native view_image currently fails.
No running UnrealEditor process was found at initial inspection.


## Evidence recorded 2026-09-19
- DebugGame builds: 124238440, 124943927 under Saved/BuildArtifacts.
- Construction tests: Saved/BuildArtifacts/20260919-124704698-automation-Hansa.UI.ConstructionProgression (3 passed).
- Recipe/save tests: Saved/BuildArtifacts/20260919-125008019-automation-Hansa.ArtisanProduction (1 test with every recipe/input/workforce case passed).
- Authored-tier test: Saved/BuildArtifacts/20260919-121925686-automation-Hansa.Editor.Definitions.ConstructionTierAuthoring.
- 9 individual ImageGen masters: SourceArt/UI/ArtisanProduction, 1254x1254 RGBA each.
- 117 proportional display variants: Content/Hansa/UI/ArtisanProduction, 16 through 160px.
- Display-size QA: SourceArt/UI/ArtisanProduction/display-size-review.png. Clean silhouettes on navy; goods recognizable at 24-32px; building trade details readable at card sizes. Building icons must retain native text labels; 20px is only a supporting glyph.
- Exact prompts beside masters; cropping/resampling dimensions and SHA256 recorded in display-variants.json.
- Workshop source: SourceArt/Generated/Buildings/ArtisanProductionV1. Three rendered revisions inspected at native1600x1000.
- Revision1 issues: regular charcoal sphere rings; floating hide hang points; smithy anvil block shape; shoemaker trade cue weak.
- Revision2: irregular smooth clamp, hanging cords, anvil horn/hammer, shoe sign; real timber braces added.
- Revision3: charcoal output heap/vent, forge braces, fleshing beam, finished shoe display. Family and individual renders retained.
- Portable FBX and GLB both clean-reimported with exact bounds, triangle counts, UVs/normals and decoded images checked. Evidence: evidence/reimport-fbx.json and reimport-glb.json.
- Reused approved native ImageGen lime, oak and clay surface masters. Procedural PBR microstructure; 1024-square physical material bakes at 2m swatch scale, not resized source masters.
- Workshop architecture is an original interpretation using the existing Lübeck material family, not a reconstruction of a documented medieval tannery or smithy.

## Review catalog and save policy
ArtisanProductionCandidate is an explicit non-Shipping review launch option. The accepted catalog remains pinned. Candidate saves require the matching candidate registry, with explicit incompatibility errors rather than silent loading into the previous economy. No save-format change is needed; new goods use existing stable-ID inventory and production serialization.
Generated content is a review draft. Promotion requires the repository's explicit approval gate after the actual property diff, asset comparisons and in-game evidence are complete.


## Final review build
Run ./Scripts/PreviewArtisanProduction.ps1 -SkipBuild after a DebugGame editor build. This starts a separate game process with the reviewed candidate, never toggles the default catalog, and does not interrupt an existing editor play session.
- Candidate: Content/Hansa/Generated/Staging/ArtisanProductionV1 (118 data assets).
- Candidate hash after save and fresh reload: 11B70A39FD538FE7.
- Accepted core baseline remains BD2FC9656111389A.
- Exact gameplay/property diff: ArtisanProduction/candidate.json.
- Fresh-disk per-definition hash evidence: ArtisanProduction/reloaded.json.
- New resource display labels use stable localization keys. This fixed initial transient-to-disk hash drift without weakening acceptance checks.
- Models: Content/Hansa/Generated/Staging/ArtisanProductionModelsV1.
- Editable master: SourceArt/Generated/Buildings/ArtisanProductionV1/checkpoints/Workshops-r3.blend.
- Portable master/materials/FBX/GLB: SourceArt/Generated/Buildings/ArtisanProductionV1/exports/.
- Iron metallic response explicitly restored to0.8; baked roughness uses linear masks, OpenGL normal maps have green-channel conversion enabled in Unreal.
- Source LOD settings: triangle fractions1/.5/.2, thresholds1/.25/.08. Three LODs are saved; NullRHI reports zero render-data thresholds, so that field in the headless evidence is not a rendered threshold measurement.
- Collision: convex decomposition with up to4 hulls; shoemaker simplifies to1 hull.
- Actual Unreal previews: SourceArt/Generated/Buildings/ArtisanProductionV1/renders/unreal-SM_*.png.
- Actual in-game evidence: Saved/ArtisanProduction/*.png and *.tsv. Selected persistent screenshots: Docs/Images/UI/ArtisanProduction/native-*.png.
- Nine1254x1254 original RGBA icon masters: SourceArt/UI/ArtisanProduction/artisanproduction--*--default--1254x1254--v1.png. Each has its own exact .prompt.md.
- Runtime native Slate PNG variants: Content/Hansa/UI/ArtisanProduction/<Name>--<Size>.png. Names: Charcoal,RawHides,TanningBark,Leather,Shoes,Smithy,Tannery,Shoemaker,CharcoalBurner. Sizes16,20,24,28,32,40,48,56,64,80,96,112,160.
- Composed ImageGen explorations v1/v2 are references only and contain documented rejected layout details. They are not shipped or treated as accurate screenshots.

## Final validation
- Final DebugGame build: Saved/BuildArtifacts/20260919-132123373-build-HansaEditor-Win64-DebugGame.
- Final Shipping build: Saved/BuildArtifacts/20260919-132430478-build-Hansa-Win64-Shipping.
- Production tests: Saved/BuildArtifacts/20260919-132442971-automation-Hansa.ArtisanProduction (2 passed).
- Construction progression: Saved/BuildArtifacts/20260919-132302035-automation-Hansa.UI.ConstructionProgression (3 passed).
- Native game flows: Saved/ArtisanProductionFinal1280.log, ArtisanProductionFinal1920.log, ArtisanProductionFinal2560.log, ArtisanProductionFinal3440.log; each RealViewport and RealWorkshops passed.
- Large-text caption regression: Saved/ArtisanProductionCaptionFix1280.log passed, screenshot inspected after correction.
- Fresh catalog commandlet: Saved/ArtisanProductionReloadClean.log, exit0. Uses the existing VerifyMediaShipping.ps1 commandlet-only GameFeatureData workaround.
- Shipping executable/receipt exclusion audit passed; final audit path appended below. No full package cook is claimed.

## Remaining release gates and limitations
Generated assets and economy definitions remain staged under AGENTS.md's explicit promotion gate. They are usable via the review launcher but not enabled in the default game catalog.
Promotion must copy reviewed model/material assets into the final /Game/Mesh family, bind accepted definitions, regenerate the accepted golden catalog and save-compatibility policy, and run the final cooked-package exclusion/dependency audit. Review saves intentionally reject the previous catalog; no silent migration is implemented.
Model trade layouts are historical interpretations, not surveyed medieval reconstructions. Windows are opaque approximations; interiors and animated workers are not supplied.
The capture city is the existing simplified MVP test map. Existing top-bar density at1280x720 with1.4x UI scale remains a broader HUD limitation; the new construction tray retains scrolling navigation and readable cards.


## Final audit additions
- Final Shipping executable/receipt exclusion: Saved/BuildArtifacts/20260919-132824270-shipping-exclusion-Win64/result.json (passed).
- Inspector regression: Saved/BuildArtifacts/20260919-132556013-automation-Hansa.UI.Inspector (6 passed).
- Native shoemaker label clipping at1280 large text was fixed by MinDesiredHeight plus an explicit native wrap width; screenshot re-inspected with both lines fully visible.
- Shared GlyphForGood now covers household needs, market demand and top products. No duplicated per-view list omits Shoes.
- The first broader market run exposed pre-existing ten-good and nonempty text-glyph assertions. The fixture contains13goods and the uninitialized baseline14; tests now compare the fixture's authored registry and verify the real Grain icon. Final rerun result is recorded below.
- Active editor play session was preserved. LOD/collision finishing used an isolated headless editor instead.

- Market final rerun: Saved/BuildArtifacts/20260919-133034881-automation-Hansa.UI.Market (6 passed).
- Final changed-GUI whitespace check passed.


## Normal-game activation — 2026-09-19

The user reported that Production → Craftsmen was empty and requested a fix. Root cause: normal startup still used catalog v27; Smithy had bShowInConstructionMenu=false and no chain assignment, while Tannery/Shoemaker existed only in the opt-in review catalog. Removing lower-tier duplication therefore left no Craftsmen choices. The UI tier filter itself was correct.

The reviewed chains are now promoted into catalog v28 (118 definitions, hash FC365BE5C93F9CA0). Production → Craftsmen offers Tools → Smithy and Shoes → Tannery → Shoemaker. Charcoal remains a Day Laborers supply chain. Normal New Game needs no candidate flag. The prior staged snapshot remains immutable.

Production models/materials/textures are under Content/Mesh/hansa-artisan-production (44 packages); 19 changed/new Core definition packages were installed with backups. The promotion commandlet checks the exact baseline and reviewed fingerprints, validates the proposed catalog before writes, replaces internal model/material/texture references, and supports a dry run. Fresh-disk reload and the v28 golden manifest prove the installed identity. Full v27 field-level reconstruction is checked before all older lineage tests.

Save policy: changed recipes and needs require New Game. Prior save files are retained; the normal registry hash guard rejects incompatible catalogs. No checksum or compatibility guard was weakened.

### UAT profile and issue ledger

Product: local Unreal game/editor. Role: ordinary New Game player. Flows: open Craftsmen, select Tools/Shoes, confirm absence of labor-chain duplication, place and inspect four workshop models, save/reload, reject prior catalog saves. Native capture scripts use the real frontend and semantic actions, with no ArtisanProductionCandidate flag. The isolated test map is L_Lubeck_MVP.

| ID | Severity | Observed | Acceptance | Status |
|---|---|---|---|---|
| CRAFT-001 | P1 | No production choices in normal Craftsmen tab | Normal startup exposes Tools/Smithy and Shoes/Tannery/Shoemaker; labor chains stay separate | Verified |

Components: existing construction shell, category/tier controls, good selectors, workshop cards, chain arrow, empty state and production inspector; native selection/focus/locks/accessibility states retained. Existing individual ImageGen icons are reused unchanged. No new raster generation, dimensions, prompts or visual style.

Verification:
- DebugGame and Development builds passed.
- Hansa.UI.ConstructionProgression: all three tests passed, including normal-catalog Tools/Shoes assertions.
- Hansa.UI.BuildMenu.CatalogAndLockedReasons passed after updating pre-existing obsolete catalog/cost expectations.
- Hansa.Integration.Authoring.EconomicAssetReload passed: current manifest, reverse ordering, and exact predecessor fingerprints.
- Hansa.ArtisanProduction: both production/input-blocker/connected-chain/mid-batch save tests passed.
- RealViewport and RealWorkshops passed at 1280×720 and 1920×1080 without a candidate flag. Includes controller focus, large text/high contrast, 80% and 140% scale, placement and actual production mesh identity.
- Inspected original-resolution Craftsmen/Shoes 720p and workshop/inspector 1080p captures. Labels and chain cards are legible; production materials resolve.
- Production-reference audit and targeted Shipping cook/dependency audit passed (Saved/BuildArtifacts/20260919-142852341-media-shipping-cook and 20260919-142908725-media-shipping-audit). This is the existing focused map cook audit, not a full packaged release playthrough.
- The first cook hit an MCP port collision; isolated cook/audit processes now disable the unused MCP server.

The broader BuildMenu suite also exposed unrelated existing expectations/failures in ArtisanConstructionPlaytest, BakeryFitsBesideRoad, SemanticsShortcutsAndFocus and ShorelineBuildingsUseAuthoritativeTarget. These concern residence eligibility, bakery inputs, focus families and shoreline targeting; they are outside this catalog activation. The focused catalog, tier and native artisan flows pass.

Persistent evidence: Docs/Images/UI/ArtisanProduction/normal-craftsmen-shoes-1280x720.png, normal-craftsmen-tools-1920x1080.png, normal-tannery-1920x1080.png. Provenance and production asset hashes: NormalGame/promotion.json. Older staged-review statements above describe the earlier phase and are superseded by this activation record.

Final activation checks: all 13 Hansa.Integration.Save tests passed (20260919-143025667), including runtime continuation and atomic historical-catalog rejection. Shipping build/receipt/executable exclusion passed (20260919-143024890); final regular Development build passed (20260919-143024153). The editor was reopened on the original surveyed Lübeck map after the user explicitly authorized stopping the previous play session. Existing save files were not modified.
