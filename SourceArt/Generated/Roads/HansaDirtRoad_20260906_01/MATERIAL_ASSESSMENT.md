# Material gap ledger

| Material / view | Reference observation | Render observation | Gap / cause | Correction | Evidence / status |
|---|---|---|---|---|---|
| Compacted dirt, source close-up | Fine sandy ground and small sparse gravel; warm sunset cast | Neutral dry ochre/beige mineral surface | Exact neutral albedo unknown from lit photo | Built-in synthetic base color, no photograph pixels; neutral light | v4_Surface.jpg, final_Raking.jpg; appearance interpretation |
| Road profile | Shallow irregular wear, broad soft ground | v1 crown and ruts overly pronounced | Height and narrow wear width | Half crown; broader, shallower wheel depressions | v1/v2_Surface.jpg; improved |
| Shoulder | Gradual irregular soil/grass transition | v2 dark edge too continuous | Vertex color attenuation | 43% to 24% attenuation | v3_Kit.jpg; improved; terrain blend remains open |
| Wheel wear | Tracks vary along road | Bright uniform parallel lines | Constant track position/brightening | Faded positional wander, weaker brightening | v4_Kit.jpg; improved |
| GLB / FBX | Same appearance target | GLB retains images and colors; FBX shader graph loses Multiply | Format shader limits | Documented map and vertex-color reconstruction for FBX | reimport_glb/fbx_Kit/Surface.jpg; checked |
| Unreal fine grain | Photograph is smoother at distance | High-frequency sparkle/noise at kit distance | Non-power-of-two base-color mip limitation; fine normals | Reduced normal XY scale to 0.08 | unreal_Kit.png; partial improvement, visual acceptance open |
| Modular edges / landscape | Road merges into grass/earth | Isolated surfaces expose boundaries, spline intentionally elevated | No receiving terrain blend | Lowered mesh shoulders and tapered end supplied; landscape integration still required | unreal_Spline.png; not production-complete |

Normals and roughness are synthetic physical channels, independent of pigment brightness; not measured scans. Source/image memory, LOD and package acceptance are separate from these visual observations.
