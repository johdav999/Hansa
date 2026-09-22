# Hansa overall design instructions

Established 2026-09-17. Read this document before design work and use the specialized references below for the area being changed.

## Design direction

Maintain a coherent Hanseatic merchant-city identity across the game, GUI and imagery. Favor historically grounded materials, clear silhouettes, restrained ornament and readable information. Keep the world visible and make player actions, economic values and feedback easy to understand.

Preserve the established visual language and shared style tokens. Extend existing systems rather than introducing a separate palette, typography family or interaction style in an individual feature. Match new work to its approved asset-family reference and to actual in-game presentation.

A user's latest explicit direction takes precedence. Record accepted changes in the appropriate design document so later work follows the same direction. Distinguish approved design requirements, exploratory references, implemented behavior and known limitations.

## Required specialized references

| Area | Authoritative document |
|---|---|
| GUI layout, palette, typography, frames, ornaments, icons, states, responsive behavior and accessibility | [Docs/UIDesignBrief.md](Docs/UIDesignBrief.md), especially **Current GUI style specification — 2026-09-17** |
| ImageGen, component artwork, provenance, scaling and visual QA | [Docs/UIAssetWorkflow.md](Docs/UIAssetWorkflow.md), plus the GUI resizing exception in [AGENTS.md](AGENTS.md) |
| Integrated game/editor/automation MVP scope and acceptance | [Docs/MVP.md](Docs/MVP.md) |
| Editor tools, schemas and AI-assisted authoring architecture | [Docs/EditorArchitecture.md](Docs/EditorArchitecture.md) |
| Authoring-editor MVP scope | [Docs/EditorMVP.md](Docs/EditorMVP.md) |
| Repository workflows and applicable skills | [AGENTS.md](AGENTS.md) |

For GUI work, read the GUI brief and asset workflow completely. The current GUI specification in the brief supersedes its older conflicting layout directions. Keep detailed GUI rules in that document; this file is the overall entry point.

## Current GUI direction

Use the newly implemented reference style: one continuous nearly black navy top bar, colored icon-first metrics, compact left alert tablets, a bottom-left minimap, an illustrated bottom-center construction tray and a right-side details panel. Use fine brass double frames, engraved corner ornaments, warm linen information surfaces and the established colored icon family.

The [GUI brief](Docs/UIDesignBrief.md) contains the complete component inventory, palette, typography, dimensions, responsive rules, interaction states and asset requirements. Use its approved reference and actual game captures to guide future changes.

## Functional and visual integrity

- Build interactive layouts, text and changing data with native Slate/UMG widgets. Keep decorative artwork separate from functional surfaces.
- Preserve authoritative simulation meaning, existing actions, localization space, keyboard/controller access, readable focus and accessibility settings.
- Use real game data. Show unavailable states honestly; illustrative reference numbers do not define gameplay values or features.
- Preserve aspect ratio and asset provenance. Follow the specialized generation workflow and approved scaling exceptions.
- Verify visual changes in the actual game at supported resolutions and UI scales. Check interaction as well as appearance, and record remaining limitations explicitly.
- Keep world-asset work consistent with its relevant terrain, building or vegetation contracts and applicable skills. GUI artwork conventions do not replace those asset-specific requirements.

## Reference and evidence locations

- [Approved GUI reference](Docs/Images/UI/ReferenceHud/approved-reference.png).
- [Implemented GUI capture](Docs/Images/UI/ReferenceHud/referencehud--ingame--default--1920x1080--v1.png).
- [GUI implementation and verification report](Docs/Development/ReferenceHudImplementation.md).
- GUI references: `Docs/Images/UI/<Feature>/`.
- GUI source artwork and prompt records: `SourceArt/UI/<Feature>/`.
- GUI runtime assets: `Content/Hansa/UI/<Feature>/`.

Update this file when the overall design direction or document hierarchy changes. Update the GUI brief when GUI details change, and preserve consistency between their summaries.


### Artisan production construction ownership — 2026-09-19
The explicit authored construction tier owns each production card. Charcoal burner's hut belongs to Day Laborers; Smithy, Tannery and Shoemaker belong to Craftsmen. Shoes selects Tannery then Shoemaker. Tools selects Smithy; charcoal is a separate Day Laborers supply chain. A lower-tier ingredient or helper workforce does not duplicate a workshop under another tier. Legacy definitions retain their existing chain classification until deliberately authored.
Reuse the existing navy/brass construction tray, native states and generated colored icon family. Card height must grow with wrapped localized/large-text labels; do not clip labels to a fixed two-line allowance. Artisan resource and workshop art/provenance is recorded in Docs/Development/ArtisanProduction.md. The reviewed gameplay catalog is now integrated into normal New Game as catalog v28 following the user-requested Craftsmen availability fix; see Docs/Development/ArtisanProduction.md for promotion, save compatibility and evidence.

### Textile production review candidate — 2026-09-19

Extend the same construction and inspector language with three Craftsmen end-product selectors: Linen clothing (Weaver → Tailor), Candles (Chandler), and Rope (Ropewalk). The Ropewalk exposes hemp and flax as two selectable recipes, never as a combined input. Keep recipe labels, quantities, progress and shortage messages native; use individual generated good icons and building illustrations only for the pictorial layer. Four historically grounded models are staged for playable review. The accepted catalog remains unchanged until the candidate, assets and actual game flows are explicitly approved. See [Docs/Development/TextileProduction/README.md](Docs/Development/TextileProduction/README.md).
