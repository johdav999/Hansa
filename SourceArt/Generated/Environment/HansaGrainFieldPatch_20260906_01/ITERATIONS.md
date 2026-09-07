# Render–inspect–correct log

| Cycle | Inspected before | Observed defect | Correction | Inspected after |
|---|---|---|---|---|
| 1 | renders/v0_hero.jpg | Sparse canopy; ears weak at strategy distance | Increase 1,024 to 1,600 stalks and add short awns | renders/v1_hero.jpg, renders/v1_close.jpg: denser crop with identifiable ears |
| 2 | renders/v1_hero.jpg | Upright stems and ears form an overly rigid stand | Increase variation in stem lean and ear-axis lean | renders/v2_hero.jpg: varied silhouette, less rigid canopy |
| 3 | renders/v1_close.jpg and v2 hero | Angular spikelets, wide leaves, excessive yellow saturation | Six-sided rounded husks, welded smooth normals, narrower leaves, reduced saturation | renders/v3_hero.jpg and renders/v3_close.jpg: rounded seeds and dry straw appearance |
| Export | first clean GLB check | Wind action present but extra static clips prevented immediate playback | Disable forced static sampling and merge to one named loop | glb_verification.json: motion, root lock, exact closure all pass |
| Materials | source v3 and baked animated hero | Potential loss of procedural shader layers on export | Bake base color, independently authored roughness and tangent normals at native 1254 square size | glb_reimport_close.jpg and fbx_reimport_hero.jpg: material families and shape retained |
| Tiling/wind | animated source | Need repeatability and motion evidence | Synchronize eight-second periodic phase, flat separate soil | tiled_2x2.jpg; wind_1.jpg and wind_49.jpg; turntable_49.jpg inspected |

Unreal iterations blocked by offline local MCP; none claimed.

Final animation portability: normalized both morph channels to 0–1 weights, preserving sampled source poses within 2e-6 m. Reimport verification checks every quarter-cycle, including the previously negative halves; see wind_normalization.json and glb_verification.json.
