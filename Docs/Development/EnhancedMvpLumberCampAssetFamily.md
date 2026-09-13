# EMVP-P12 — Lumber Camp asset family

Status: **promoted and bound following user approval on 2026-09-08**. See
[promotion and verification report](EnhancedMvpApprovedPromotionP12P15.md).
Playable and Shipping acceptance remain open. The original staged handoff below is
retained as historical context; its pending-approval statements are superseded.

Source delivery: `SourceArt/Generated/Buildings/HansaLumberCamp_P12_20260908/`.
Start with its README.md, EVALUATION.md, ITERATIONS.md, MATERIAL_ASSESSMENT.md,
MATERIAL_INVENTORY.md, PROVENANCE.md and reference_manifest.csv. Final native Unreal
review captures and the clean-export turntable are in renders/; packed Blender,
GLBs, centimetre-correct *_UE.fbx files and 1024-square PBR maps are in exports/.
Three ImageGen 1254-square masters and exact sibling prompts are retained in textures/.

Staged review level: `/Game/Hansa/Generated/Staging/LumberCamp_P12/L_LumberCamp_Review`.
Staged actor: `BP_LumberCamp_Review`, native parent `AHansaLumberCampPresentation`.
Only the `MeshesCM` import revision is scale-correct; `Meshes` is rejected diagnostic
history. Staging is deliberately ignored by Git and excluded from cooking. Source
assets/scripts are versionable and reproduce the candidate without provider calls.

Five roles: work hall (Presentation.Building.LumberCamp), plus log pile, rough-hewn
timber, tool shelter and stump/slash (Presentation.Prop.Lumber.Input). Components
have stable role tags; actor tick/collision/navigation are disabled. Placement remains
the existing 3x3-cell contract. No forest is baked into the asset and no new forest
prerequisite, recipe or economy rule is introduced.

Verified: packed-master reopen; clean FBX/GLB import; three source correction cycles;
Unreal save/reopen; unit-scale and +/-580cm inset; PBR slot assignment; three LODs and
simple mesh collision; neutral 25/65/120m and warm 25m captures. Two local Lumber Camp
tests and six placement regression tests passed. The staged test is user-instigated
because it requires the transient candidate import. Ordinary CI retains asset-free
presentation-state coverage. See EVALUATION.md for exact limitations and evidence.

Production promotion requires explicit review approval under AGENTS.md. The existing
P02 manifest and Building.LumberCamp production references intentionally remain
unchanged until then. After approval, promote to `/Game/Mesh/hansa-lumber-camp`, bind
the stable roles, run the actual placement/production/save-load flow, and audit
Shipping dependencies. Do not infer in-game completion from the review fixtures.
