# Enhanced MVP Hop Farm Asset Family

## Outcome

The hop farm is a modular production asset family for the Hansa strategy camera. It combines a new pole-grown hop field and harvest props with approved grain-farm components instead of duplicating the farmstead shell, construction state, furrow geometry, or established material language.

## Component inventory

| Component | State/role | Source |
|---|---|---|
| Farmstead shell | Ready/working shell | Reused `SM_HansaGrainFarm` |
| Construction scaffold | Construction state | Reused `SM_HansaGrainFarm_Construction` |
| 4 m soil/furrow tile | Field ground | Reused `SM_HansaGrainField_Furrow_4m` |
| Center hop module | Mature center tile | New production mesh |
| Edge hop module | Mature boundary tile | New production mesh |
| Corner hop module | Mature corner tile | New production mesh |
| Drying rack and baskets | Harvest/status storytelling | New production mesh |
| Full assembly | Visual inspection only | Staging-only reference |
| Weathered oak and linen | Pole/prop material bindings | Reused grain-farm materials |
| Foliage and cones | Crop material bindings | New production materials |

Runtime state presentation should be assembled natively from these pieces. The full assembly is not a shipping composition and does not replace the existing building-definition presentation contract.

## Production paths

- New production root: `/Game/Mesh/hansa-hop-farm/`
- Reused grain-farm root: `/Game/Mesh/hansa-grain-farm/Final/`
- Staging-only full assembly: `/Game/Hansa/Generated/Staging/HansaHopFarm_20260912_01/Meshes/SM_HansaHopFarm_FullAssembly`
- Editable source package: `SourceArt/Generated/Buildings/HansaHopFarm_20260912_01/`

## Verification

- Four production meshes, two materials, and six textures are promoted and saved.
- Each production mesh has three LODs, zero collision primitives, and Nanite disabled.
- FBX and GLB clean reimports match triangle counts and dimensions within floating-point tolerance.
- Production mesh/material dependencies contain no staging references.
- Center, edge, corner, harvest-prop, field-join, full-assembly, and Unreal asset captures passed visual inspection.
- The selected ImageGen source remains at its native 1254 x 1254 dimensions with a sibling prompt record and provenance hash.

## Remaining integration work

The model family is ready for gameplay/editor composition, but this task does not change `DA_Building_HopFarm`, seasonal or wind behavior, HLOD policy, or the building-state presentation schema. Those changes should be handled as a separate feature-parity implementation stream under the editor/game contract.
