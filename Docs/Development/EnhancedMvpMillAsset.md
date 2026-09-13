# EMVP-P09 — production mill revision

The bread chain retains `Building.Mill` and `/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated`. P09 revises its existing tower-mill source instead of introducing a competing building. The two production meshes are `/Game/Mesh/hansa-mill/P09/Meshes/SM_Mill_Body` and `SM_Mill_Rotor`; the eight approved production materials remain under `/Game/Hansa/Core/Buildings/Windmill/Materials`.

## Candidate audit and decision

| Existing work | Decision |
|---|---|
| `HansaMill_20260906_01` | Retain as historical source, not the selected production asset. Its timber post-mill type is plausible, but its principal photographic example is a much later Estonian mill; changing to it would discard the user's accepted tower and rotor integration. |
| `HansaTowerMill_20260906_02` | Retain the user-selected masonry, roof, openings, four lattice sails and original ImageGen material families. The exact photographed building/date remain unknown. |
| `HansaTowerMill_CapFit_20260906_03` | Preserve the corrected roof seating and crown clearance. Do not restore the earlier narrow cap. |
| `HansaWindmillAnimated_20260906_04` | Revise this master: preserve the native rotor separation and bearing, normalize mesh transforms, ground the body, reduce geometry, create LODs and verify full rotation. |

Tower windmills existed in Europe in the fourteenth/fifteenth centuries; this supports the building type, not the date or every detail of this particular form. The exact cap, windows, dimensions and condition are a game interpretation of the accepted user reference, not an authenticated 1400 Lübeck reconstruction. No automatic fantail, new cloth simulation, rigging system or provider integration is added.

## P07 family addendum

- Blender metres map to Unreal centimetres; actor/component scale stays `(1,1,1)`. Body tight bounds are approximately 800×819.5×1304.27 cm, fitting the 1160×1160 cm usable 3×3 plot. The body ground pivot is zero.
- Preserve this existing rig's **+Y entrance/front** as the explicit mill-family exception to default +X. The rotor's local Y/Pitch axis and child location `(0,399,1083.5)` cm are tested. Placement quarter-turns remain owned by the existing projection.
- The double-leaf goods entrance is 165 cm wide and approximately 212 cm clear to its arch apex, a cargo-door exception to the ordinary 85–120 cm single-person width. Windows are not asserted to be measured floor levels; the internal mill machinery/storeys are not modeled.
- Four sails have deliberate visual overhang, excluded from grounded footprint/collision fitting. The 72-position optimized-mesh sweep has no body intersections outside the intentional shaft bearing. Minimum moving-sail height is 258.3 cm; maximum swept height is 1908.7 cm. Rest-pose height alone is not a clearance bound.
- Both parts use three conventional LODs, 50%/25% reduction targets, thresholds `1/.32/.12`, no Nanite. The body has simple convex collision; rotor mesh has none. Runtime presentation components are visual-only, with selection/navigation owned by the managed projection.
- Native 1024×1024 portable PBR maps are reused unchanged. Masonry's broad wall allocation is 409.6 colour pixels/m; narrow timber, brick, stone, paint and trim use 1024 pixels/m as documented hero/detail allocations. This intentionally exceeds the default 25% within-asset variance; it does not resize a raster or claim uniform density.
- The accepted late-summer grey timber, damp lime/rubble, dull brick and faded sage paint remain unchanged. No new snow/season variant is included.

## Runtime and parity

Ready production uses the existing local-Pitch 36°/s movement (6 rpm). Blocked and constructing projections stop it without resetting phase; ready resumes its authored rate. Nested components cannot reacquire collision/navigation after Blueprint reconstruction. Existing stable selection, footprint, rotation, construction hide/reveal and teardown remain in place. Mill recipe inspection asserts `Good.Flour` output.

No new gameplay data model, editor field, migration, catalog revision, save format, provider job or worker dependency is introduced. Existing reflected `PresentationActorClass` schema, validation, impact/hash handling and staging rejection remain the authoring path. The class path and definition asset are not changed by this revision; only approved visual dependencies are revised.

## Reproduction and evidence

Source package: `SourceArt/Generated/Buildings/HansaMill_P09_20260907/`.

1. `scripts/revise.py -- 0`, `-- 1`, `-- 2`: baseline, normalized editable source, optimized packed source and native 900×900 reviews.
2. `scripts/export_portable.py`: explicit portable material translation, two FBXs and animated GLB. Run after revision; generic FBX cannot encode the source's vertex-colour multiply graph.
3. `scripts/verify.py -- fbx` and `-- glb`: clean import and matched-camera render; `scripts/clearance.py`: full sail sweep.
4. Import the two FBXs through the discovered Unreal MCP mesh importer into the P09 root. `scripts/prepare_unreal.py` with `-P09MeshesOnly` finalizes those assets in a separate full editor using legacy FBX with vertex-colour Replace, explicit slots, LODs and collision. The default Interchange adapter does not honour all legacy FBX options.
5. Update the existing main/rotor Blueprint roots **and child-actor template**, preserving their paths. Save only those two packages in the connected editor; do not try to overwrite a package locked by another editor. `scripts/preview_unreal.py` saves/reopens the isolated preview and records native distance views and the production dependency closure.

The preview is `/Game/Hansa/Generated/Staging/Mill_P09/L_Mill_Preview`; its floor/lights and captures are review-only. The existing staging cook exclusion applies. No reference from the production mill may point back to this map or staging. Dependency-closure evidence is not a substitute for a complete Shipping cook.

See the package's `EVALUATION.md`, `ITERATIONS.md`, `PROVENANCE.md`, `material_assessment.md`, `reference_manifest.csv`, `comparisons.html` and `evidence/` for measured acceptance and limitations. The general construction placeholder and final P30 city lighting are not redesigned by this focused asset revision.

## Verification outcome and wider blocker

The three focused P09 tests pass: mill Blueprint presentation, authored placement ghost and save round-trip continuation. The full Development editor builds with MSVC 14.44, non-unity and a command-line-only `/Od` workaround for an internal compiler error; no build settings are persisted. Native review captures are 2253×772 and source reviews 900×900, without raster resampling.

The separate `Hansa.Integration.Authoring.EconomicAssetReload` gate remains red: `HSA-REGISTRY-038` at `Good.Bread.ConstructionChain`. Read-only inspection shows **GrainFarm revision 3 is hidden from construction and has blank chain identity/stage 0/count 0**; Mill is correctly visible at stage 2/3 and Bakery at 3/3. This pre-existing P08/metadata/catalog repair is not applied by P09. The diagnostic is saved in `evidence/bread_chain_diagnostic.json`; catalog validation now logs its actual causes before downstream count/hash failures. Restore the farm's approved construction metadata and perform the catalog-versioning review before claiming the entire enhanced bread-chain release gate passes.

The user's running DebugGame editor was left open and its terrain level untouched. It still has older loaded runtime/schema modules; restart into the rebuilt Development editor, or rebuild/restart DebugGame, to load the C++ changes. A complete Shipping cook and final gameplay performance/lighting pass remain separate release gates.
