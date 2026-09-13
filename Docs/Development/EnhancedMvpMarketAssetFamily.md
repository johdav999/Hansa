# EMVP-P15 — civic Market presentation

Status: **promoted and bound following user approval** (2026-09-08).
See [promotion and verification report](EnhancedMvpApprovedPromotionP12P15.md).
Playable and Shipping acceptance remain open. The original staged handoff below is
historical; its pending-approval and unchanged-binding statements are superseded.

P15 is not fully accepted: explicit asset promotion approval and playable-map acceptance remain outstanding. No production definition, canonical visible-content manifest entry, balance value, or scenario placement was changed.

## Delivery and review

- Source/evidence: `SourceArt/Generated/Buildings/HansaMarket_P15_20260908/README.md`.
- Editable packed master: `exports/HansaMarket.blend`; six separate FBX and GLB modules; eight portable PBR material families.
- Unreal staging: `/Game/Hansa/Generated/Staging/Market_P15/`.
- Review Blueprint: `BP_Market_Review`; review map: `L_Market_Review` (saved and reopened).
- Native type: `AHansaMarketPresentation`; no hard-coded media paths, provider code, ticking, replication, or simulation writes.
- Golden review contract: `Tests/Golden/enhanced_mvp_market_asset_family_v1.json`.
- Validation: `Scripts/ValidateEnhancedMvpMarketAssets.ps1`.

| Manifest role | Native component / geometry | Promotion destination, only after approval |
|---|---|---|
| `Presentation.Building.Market` | CourtHall: original open timber civic hall, clay roof, paved court | `/Game/Mesh/hansa-market/SM_HansaMarket` |
| `Presentation.Prop.Market.Trade` | Stalls ×2; Awnings ×2; Scales; Baskets ×2; Cargo | `/Game/Mesh/hansa-market/Props/SM_HansaMarket_*` |

Stalls, awnings, and baskets each use one instanced component. Six mesh components render nine module instances. All component and instance scales are one. Geometry is authored in metres, imported as centimetres with source Y mirrored. The assembled bounds are approximately 1080 × 1080 × 606 cm, leaving 60 cm per edge inside the existing 1200 × 1200 cm plot. No selection or traffic collision comes from these cosmetic components. The existing world projection owns selection/placement.

Construction displays the authored hall/court while hiding trade equipment. Ready and Blocked retain the same completed equipment: representative cargo does not claim current inventory, sales, prices, throughput, or service availability. Dynamic text and currency are not baked into any texture. No P35 effects were added.

## Definition parity and service limitation

`Building.Market` remains road-required, 3×3, storage 50000 milliunits, Engine cube fallback and no PresentationActorClass. Original and final definition SHA-256:
`615AD3ABD0CB2A765853415B5E933BEF556A3A0E775EA85F161F0F12A31F0F27`.

There is **no authored Market service-radius field** in the current definition/compiler schema. `HansaPopulation.cpp` derives market access from matching city inventory and city market records. Cohort `ServiceAccessBasisPoints` is consumed from simulation state; it is not recalculated from nearby Market buildings. The seeder's 6/2 values are workforce inputs, not coverage distances. Completed world status derives from construction and production blockers; it does not establish Market road/service coverage.

Therefore no cosmetic radius or false disconnected/service indicator was introduced. Existing access and inspector behavior can be regression-tested, but a radius-based playable coverage acceptance cannot truthfully pass. Defining a new coverage rule requires an explicit gameplay decision, then corresponding runtime/editor schema, validation, migration, impact analysis, automation, and balance fixtures in the same implementation stream.

No gameplay schema changed in P15. Native mesh component metadata is exposed to the editor. The staged-asset test resolves the real stable definition with temporary in-memory bindings, restores them on scope exit, verifies dimensions/road requirement, and confirms the production resolver rejects the staged Blueprint. It never saves that definition.

## Acceptance and promotion gate

HansaModels produced research-grounded original geometry, built-in ImageGen canvas color, independent roughness/normal detail, four inspected correction cycles, clean FBX/GLB reimports, and native Unreal captures. See the archive evaluation for measurements, comparisons, limitations, and exact evidence.

After explicit approval: promote the six meshes/materials/textures and Blueprint to canonical paths; update the real definition and both existing manifest roles together; validate references and impact; then verify selection, construction, road placement, save/load, and the agreed service semantics in the playable map. Keep staging and source tooling excluded from Shipping. P30 production dependencies are not marked satisfied by this draft.
