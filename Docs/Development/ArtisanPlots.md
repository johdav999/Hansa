# Artisan street plots — 2026-09-16

The Artisan house construction card places one 16 x 8 m residence (4 x 2 grid cells). At equal weights, the persistent parcel seed selects Compact (plain brick gable and storage shed) or WorkYard (stepped brick gable and rear workshop). The seed comes from city ID and saved building ID/generation. Preview uses the next authoritative parcel seed; construction completion, projection rebuild, rotation and save/load do not reroll it. Layout IDs remain stable.

The complete plot is reserved at placement. Every architectural mesh is scale one. The street house faces the road, with a pedestrian passage down its side to a small rear yard. Native validators check mesh envelopes, every access node/link, front road contact, overlap, footprint and population tier. Extra structures are cosmetic instances of one logical building: the plot retains the legacy artisan capacity, construction cost, upkeep and build duration, and creates no workshop production.

## Test switch

In Config/DefaultEngine.ini:

    [Hansa.Housing]
    UseArtisanPlots=True

Set False and restart/reload the construction catalogue to restore the legacy artisan card. Existing placed plots remain intact; this switch does not remove definitions or change the catalogue hash. The independent AllowDirectArtisanResidence playtest override still controls direct construction of the legacy upgrade-only house.

Existing 8 x 8 m houses and their upgrade paths are unchanged. The larger plot has new stable IDs, Building.Residence.Artisan.Plot and Compound.Artisan.Plot, so no occupied legacy parcel expands. No new upgrade stages are introduced in this deliverable.

## Authoring and assets

- Building: /Game/Hansa/Core/Buildings/ArtisanPlots/DA_Building_Residence_Artisan_Plot
- Compound: /Game/Hansa/Core/Compounds/ArtisanPlots/DA_Compound_ArtisanPlot
- Layout source: SourceArt/Generated/Buildings/LubeckUrbanHousing_20260916/artisan-plots.json
- Native review map: /Game/Hansa/Developer/GenerationPreview/UrbanHousing_20260916/L_ArtisanPlots
- Existing approved plain and stepped meshes, materials and textures: /Game/Mesh/lubeck-urban-housing-r01
- Existing workshop, shed and fence meshes: /Game/Mesh/labour-housing-kit/Meshes_R08

No new raster artwork or mesh generation. The existing ArtisanHouse ImageGen icon and all native card states are reused. The compound schema permits Laborer and Artisan tiers and still requires the owning residence to match. Metadata, validation, binding impact and interchange apply to both. JSON export preserves localized text namespace/key; existing plain-string JSON continues to import. No serialized property, simulation field, provider or save-format change is introduced.

The HansaArtisanPlots commandlet authors and validates the two layouts, checks all prior fingerprints and refuses to overwrite existing assets. The full catalogue manifest records the two additions; the reload test reconstructs the prior catalogue by removing only them. The exact-hash save policy requires a new game for older catalogues; no save is deleted or silently migrated.

Developer review maps and Generated/Staging remain excluded from cooking. The reviewed house asset folder is now a production dependency of the artisan plot, so its former whole-folder NeverCook rule is removed.

## Verification

18 focused native tests passed, including four constructed plots, exact save/load state and selected-layout preservation, all compound regressions, authored interchange, prior catalogue reconstruction and legacy playtest switches. Development Editor and Shipping builds passed. The Shipping receipt/executable exclusion audit passed, and the native asset dependency closure contains no Developer or Generated/Staging packages. This is not a full packaged city-scale cook certification.

Both production layouts were saved/reopened and visually inspected in native 1600 x 1000 front/rear captures. Captures and dependency evidence are in SourceArt/Generated/Buildings/LubeckUrbanHousing_20260916. The existing icon is reused; there are no new raster dimensions or generation prompts.

Your currently running Unreal session uses the previous native modules. Verified replacement DLLs are staged under Saved/ArtisanPlots/VerifiedEditor, with hashes. Close Unreal and run Saved/ArtisanPlots/InstallVerifiedEditor.ps1 to install them reversibly; the installer refuses to touch a running Hansa editor and backs up the previous modules. Then reopen Hansa and start a new game. No running session has been stopped automatically. Existing detailed house geometry remains expensive; city-scale performance is not certified by this change.


The broader build-menu run has four failures also reproduced with UseArtisanPlots=False: BakeryFitsBesideRoad (Ready versus Blocked), CatalogAndLockedReasons (grain-farm cost expectation), SemanticsShortcutsAndFocus (focus-order expectation), and ShorelineBuildingsUseAuthoritativeTarget (second shoreline footprint). These are outside this plot change and are retained as explicit regression limitations. The focused acceptance run passes all 18 tests with a normal zero completion marker.

Machine-readable evidence: SourceArt/Generated/Buildings/LubeckUrbanHousing_20260916/evidence/artisan-implementation-verification.json.


### Material instancing requirement (2026-09-16)

Compound presentation uses hierarchical instanced static meshes. Every material
assigned to the artisan meshes must persist `bUsedWithInstancedStaticMeshes=true`
before gameplay or cooking. The original import omitted this flag for all eleven
`M_Urban_*` materials; Unreal logged missing InstancedStaticMeshes usage and
substituted its grey default material in PIE. Texture maps and UVs were intact.

The import script now sets the flag explicitly. `fix_material_usage.py` repairs
and recompiles the existing materials through Unreal MCP; run outside PIE to save.
`verify_material_instancing.py` checks all eleven materials and both artisan mesh
slot lists. Run its Unreal Python commandlet with `-verifyonly` in a fresh process
to prove the flags were persisted without auto-repairing them.
