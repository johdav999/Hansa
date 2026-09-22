# Construction progression selector

## Implemented behavior

The approved reference is implemented as native Slate category / tier / goods-or-building rows. Three text-only tier buttons use the existing shared navy, slate, brass and Chalk styles: Day Laborers, Craftsmen, Merchants. The user approved the composed reference in Docs/Images/UI/ConstructionProgression. Existing generated category, goods and building icons are reused unchanged; the generated screen is not a shipping texture.

Category and tier remain independent. Changing category keeps the tier; changing tier cancels road/card/stroke placement and clears its footprint and confirmation. A compatible production chain stays expanded; an incompatible one closes. Complete chains stay together, regardless of worker requirements. Remote/non-buildable city restrictions still apply.

Residences filter by the authored ResidentPopulationTierId. Laborer maps to Day Laborers, Artisan to Craftsmen, Merchant to Merchants. Stable IDs and population simulation retain their existing identities. This implements construction browsing, not a new household promotion system.

Production chains are assigned to the lowest workforce tier required anywhere in the complete chain. A later building may require some artisans without duplicating the labor-based chain under Craftsmen. Chains with laborer workforce appear under Day Laborers; chains with artisan workforce and no laborers appear under Craftsmen. Roads, storage, harbor and other non-residential infrastructure stay available across tiers. Browsing never grants technology, resources, workers or upgrade eligibility. Existing locks remain authoritative.

The accepted scenario currently has no merchant residence/population simulation content. Its Merchants tab is functional for browsing shared facilities; absent merchant-specific choices show an explicit empty-state message. Merchant-house founding and Ratsherr politics are outside this change.

## Components and states

- Shell: existing native construction tray and reference frame.
- Category row, goods cards and building cards: existing native controls and approved ImageGen artwork.
- Tier row: new text-only SHansaAction controls, reusable across three labels.
- Empty state: native localized text.
- Feedback: existing native selection, hover, pressed, disabled, warning/error, independent keyboard/controller focus and placement feedback.
- Charts/overlays/decorative images: no additions.
- Horizontal scrolling preserves access at compact widths; tier focus reveals its control.
- Semantic IDs: BuildMenu.Tiers, BuildMenu.Tier.DayLaborers, BuildMenu.Tier.Craftsmen, BuildMenu.Tier.Merchants, BuildMenu.EmptyTier.

## Editor, compatibility and impact

Filtering derives from existing typed economic definitions. There are no new serialized gameplay properties, catalogue hashes, save versions, migrations, generated data, imported assets, provider dependencies or editor/runtime dependencies. Existing schema editing, validation, import and impact analysis for residence tier and population needs are the authoring path. A need change therefore changes its tier's construction-chain browsing automatically after the ordinary catalogue reload.

BrowsingTierMask and SelectedTier are transient presentation fields. The new enum is UI-only. The source mapping names three stable tier identities but does not create PopulationTier.Merchant content. Unknown household tiers do not silently appear as shared housing.

## Verification

- DebugGame editor build succeeded.
- Hansa.UI.ConstructionProgression.AuthoredClassification passed.
- Hansa.UI.ConstructionProgression.SemanticJourney passed.
- Hansa.UI.ConstructionProgression.RealViewport passed at 1920x1080 and 1280x720.
- Eight native captures per resolution cover default production, both existing household tabs, absent merchant residences, shared storage, high contrast/large text, and 140% / 80% scale.
- Inspected the native 1080p default and 720p Craftsmen, Merchants and 140% captures. Tier labels and selected focus are readable; the three tabs fit and their contents do not overlap.
- Evidence: Saved/ConstructionProgression and timestamped Saved/BuildArtifacts logs.
- Existing open Development editor must load freshly built modules before this change appears in that session. DebugGame verification does not hot-reload it.
- A Shipping package was not produced for this presentation-only change; no new shipping dependencies are introduced.

Final validation also passed on the configured surveyed Lubeck terrain map at both 1920x1080 and 1280x720. Inspected the 1080p native capture on that map. The existing CompactTrayAndControllerNavigation and StableCardFocusAndChainEdges tests passed as well (four headless tests total). The final empty-state polish removes the empty card surface beneath absent merchant residences.

## Development activation — 2026-09-17

The user authorized continuation of the editor restart. The regular Development build succeeded (Saved/BuildArtifacts/20260917-215811016-build-HansaEditor-Win64-Development), and both ConstructionProgression regression tests passed against Development modules (Saved/BuildArtifacts/20260917-215827131-automation-Hansa.UI.ConstructionProgression). Hansa was reopened using the regular UnrealEditor.exe with the freshly verified modules. The earlier restart-pending note is superseded by this activation record.

## Labor-chain filtering correction — 2026-09-17

Production browsing no longer derives tier visibility from household consumption.
That approach duplicated labor-based Bread, Beer, Fish, Firewood and Planks chains
under Craftsmen because artisans consume some of the same goods. Complete chains
now use their lowest required workforce tier. An artisan requirement in a later
stage remains visible in the building tooltip but does not move or duplicate the
chain. The Craftsmen production tab shows its explicit empty state until an
artisan-only chain is authored.

The focused classification and semantic tests pass in DebugGame and Development.
A native 1280x720 capture on the configured terrain confirms the Craftsmen
Production tab contains no labor chains. Development build:
Saved/BuildArtifacts/20260917-221103186-build-HansaEditor-Win64-Development.
Development tests:
Saved/BuildArtifacts/20260917-221115843-automation-Hansa.UI.ConstructionProgression.
The regular Hansa editor was reopened with these modules.


## Craftsmen normal-game catalog correction — 2026-09-19

The prior empty-tab state is superseded by accepted catalog v28. The separately reviewed Smithy/Tools and Tannery→Shoemaker/Shoes definitions are now loaded by normal New Game, without a preview flag. Explicit ConstructionTier owns these cards; charcoal belongs to Day Laborers. See ArtisanProduction.md for the root cause, integration, save policy and real-viewport tests.
