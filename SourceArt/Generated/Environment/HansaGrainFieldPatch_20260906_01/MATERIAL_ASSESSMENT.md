# Material gap ledger

Reference photograph: University of Oslo wheat field, 850 x 638, retained for reference only. Different lighting, plant condition and framing; it is not a texture input. Compare native images in COMPARISONS.html.

| Material / view | Reference observation | Render observation | Gap / priority | Cause / uncertainty | Correction | Evidence / status |
|---|---|---|---|---|---|---|
| Straw / hero | Slender stems, irregular bend | v0 sparse, vertical stand | Rigid silhouette / high | Simplified placement and limited lean | Denser jittered placement, variable height and lean | v2 hero: improved, verified |
| Husk / close | Layered tapered spikelets | v1 angular repeated diamonds | Faceting / high | Four-sided coarse cross-section and flat normals | Six-sided husks, welded smooth shading | v3 and clean GLB close: improved, verified; exact botanical bracts simplified |
| Straw and husk / color | Pale beige-grey in overcast photo | v1 strongly golden | Saturation / medium | Synthetic straw color and sunny lighting | Lower saturation in editable shader and baked color | v3 and FBX hero: restrained mature straw; lighting remains unmatched |
| Leaf / close | Narrow, irregular bent blades | v1 wide angular leaves | Width / medium | Uniform broad ribbons | Narrow to 12 mm, keep alternation and varied lengths | v3 close: improved; blade curvature still segmented |
| Soil / tiling | Photograph does not show soil | Flat finely mottled earth | Reference unavailable / low | Artistic inferred soil | Separate native ImageGen input; independent subtle relief | Four-patch render: no raised border or exposed seam; photo match not assessable |
| Physical channels | Fine relief unmeasurable in photo | Restrained diffuse response | Physical measurement unknown | Procedural roughness/relief are artistic | Independent 0.15 mm grain relief, roughness .72–.94; bake normal/roughness separately | Raking close and reimports: no exaggerated dents or specular glare |
| Export maps | Source material is authority | GLB/FBX use baked maps | Procedural loss risk | Format translation | Bake and reconnect twelve PBR maps | Clean reimport views verified |
| Unreal | No engine evidence | No import occurred | Blocked | MCP unavailable | Start installed MCP server, import to staging, construct wind material, recapture | Open; no engine acceptance |

Both generated originals are 1254 x 1254 and were inspected without resampling. They have no text/borders or distinct large lighting gradients. Small soil aggregate shading is an artistic approximation, not measured albedo. Grain color is not converted into physical height. Texture repetition is subdued at intended distance; sparse variation and simplified close-up anatomy remain limitations.
