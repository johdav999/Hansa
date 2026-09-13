# Enhanced MVP Sawmill asset family

EMVP-P13, 2026-09-08: **promoted and bound following explicit user approval**.
See [promotion and verification report](EnhancedMvpApprovedPromotionP12P15.md).
Playable and Shipping acceptance remain open. Pending-approval statements in the
original staged handoff below are historical and superseded by that report.

Review/source package: `SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/README.md`.
Machine contract: `Tests/Golden/enhanced_mvp_sawmill_asset_family_v1.json`.

The original manual-power saw yard preserves `Building.Sawmill`'s 4x3, road-required, non-shore placement contract and `Recipe.SawPlanks`'s timber-to-planks recipe. It introduces no power, terrain, economy or identity schema changes. Its five roles are WorkBuilding, LogInput, PlankOutput, Rack and SawWork; flow props map to the existing `Presentation.Prop.Sawmill.Flow` manifest role. A raised trestle saw avoids an unsupported wheel/shoreline requirement.

`AHansaSawmillPresentation` is a read-only, no-tick runtime actor. The existing world projection forwards status to it and suppresses the construction cube for this authored assembly. No native defaults point to draft assets. Construction retains the hall; ready exposes a symbolic plank-output cue; blocked removes that cue without hiding the building. Art has no collision/navigation authority and does not own inventory.

Five meshes, three LODs each, five PBR materials and fifteen connected native maps are imported at identity scale under `/Game/Hansa/Generated/Staging/Sawmill_P13`. The Blueprint and both isolated review levels were saved/reopened. Source/portable exports, actual render corrections, references, prompt records and inspection evidence are retained in the package. Staged tests and the editor build pass; the package validator checks imported scale, plot fit, LOD/collision evidence and source hashes.

Production promotion is not implied by importing or by asking for the next numbered prompt. Neither the Sawmill definition nor P02 content-completeness status is changed to a false accepted state. Existing loader validation rejects `/Generated/Staging/` and `/Developer/` paths; the staged test confirms this. No migration is needed for a presentation-only type, but promotion must refresh and validate any affected presentation/catalog hashes through the existing editor workflow.

Pending: explicit approval, selected-asset promotion to `/Game/Mesh/hansa-sawmill`, stable definition binding, actual Lumber Camp→Sawmill runtime chain/placement/selection/save-load acceptance, and Shipping/camera/performance gates. P12 also needs approval before its production side of the chain can be bound. `L_TimberChain_Review` is an art comparison, not live simulation evidence.
