# Material comparison ledger

| Material / view | Reference observation | Initial gap | Correction and evidence | Remaining approximation |
|---|---|---|---|---|
| Masonry / front and base | Dirty grey-beige plaster with scattered exposed rubble | r0 oversized mapping; r1 too flat | Physical cylinder UVs, ImageGen plaster/rubble color, partial stone geometry and localized runoff; r3 Base and final reimports | Damage layout is not copied exactly; fine substrate relief is simplified |
| Stone / base | Stones mostly embedded in coat | r2 stones resembled attached lumps | Recessed 6.5 cm, muted hue extremes; r3 Base | Small exposed faces remain simplified polyhedra |
| Brick / openings | Worn red-brown arches and narrow jambs | Stepped box arch; overly clean surfaces | Fitted wedge bricks and new aged-brick ImageGen input; r3 Base and final FBX Base | Surrounds remain somewhat regular; no scanned erosion |
| Roof / cap | Grey staggered shingles and irregular edges | Aligned flank seams; pale timber | Staggered modeled shingles, restrained per-shingle tone, darkened weathering; r4 Roof | Cap depth/back silhouette inferred; edge wear simplified |
| Sail timber / roof close-up | Long grey stocks and thin lattice | Pale members; round hub cap | Grain-aligned ImageGen timber, darker condition, bindings/bolts, diamond iron plate; r4 Roof, engine Iron | No cloth or operational deformation |
| Sage paint / entrance | Faded green doors and frames | Flat clean coating | New worn-paint ImageGen source, separate physical relief; final Base | Paint flakes primarily color detail |
| Iron / hub | Dark plate and fasteners | Round cap silhouette | Diamond plate, rivets, rough oxidized metal; r4 Roof / engine Iron | Procedural rust/metal, no scanned corrosion |
| Glass / windows | Dark small panes with green frames | Transmission not established | Consistent reflective opaque approximation in source and engine | No true glass transmission or detailed interior |

Original inputs and repeated swatches were inspected at native size. Clean export renders preserve the visible color families and weathering. FBX's unsupported vertex-color material multiply is explicitly rebuilt, not assumed. Unreal uses separate material graphs and verified data-map settings. Source overcast and engine sun/sky lighting differ, so colors are compared as material families rather than calibrated albedo measurements. Intended use is approximately 5–30 m; very close shots expose the stated simplifications.
