Below is a sequential prompt pack. Run them in order so asset names and compound definitions remain consistent.
Prompt 1 — Audit assets and create the missing labour-housing kit
Use $hansamodels.

Create the historically grounded building kit needed for Hansa’s labourer residential compounds.

First locate the actual Hansa repository and .uproject. Read all applicable AGENTS.md files and the project’s design, asset-workflow, architecture and historical-setting documents. Inspect the existing meshes, materials, building definitions, settlement systems and residential assets before creating anything.

Goal:
A single selectable labour-housing parcel should visually contain several modest dwellings and outbuildings, resembling the supplied Anno 1800 reference in density and variation. Do not copy Anno’s architecture. Base every new structure on documented buildings appropriate to Hansa’s actual city, region and historical period.

Produce an asset-gap ledger covering:

- small one-storey labourer cottage
- narrow two-storey labourer dwelling
- paired or short-row dwelling
- rear-yard workshop
- storage shed
- lean-to extension
- privy or small utility outbuilding
- simple stable, where historically appropriate
- covered passage or gateway
- historically appropriate fences, gates and yard boundaries

Reuse suitable approved Hansa assets. Generate only missing structures or variants.

For each missing structure, follow the complete HansaModels workflow:

1. Research authoritative historical or heritage references.
2. Record observed, inferred and unknown features.
3. Establish real dimensions, massing, roof form and construction.
4. Produce an editable, modular Blender master.
5. Use ImageGen texture inputs and hybrid PBR materials according to the skill.
6. Complete at least three render–inspect–correct cycles.
7. Export and cleanly reimport GLB and FBX for verification.
8. Import the accepted asset and its materials through Unreal MCP into:
   /Game/Mesh/<asset-slug>/
9. Inspect the actual Unreal result under neutral daylight.
10. Deliver provenance, reference manifests, evaluations, iteration logs and comparison renders.

Design the kit for recombination:

- consistent real-world scale
- compatible ground levels
- reusable material families
- restrained colour variants
- modular extensions where historically defensible
- practical pivots for parcel placement
- clean collision, UVs, normals and LOD/HLOD compatibility

Do not place anything into gameplay maps yet. Do not overwrite approved assets. End with the exact Unreal asset paths and a recommended catalogue of meshes for the compound-layout system.
Prompt 2 — Implement the compound-parcel system
Implement data-driven residential compounds for labourer housing in the Hansa project.

Locate the real Hansa repository and .uproject, then read all applicable AGENTS.md, design, architecture, editor and settlement-system documents. Inspect the current building placement, road connection, selection, population, upgrade, save/load, rendering and navigation systems before changing code.

Core rule:
One gameplay parcel remains one logical residential building, but its visual representation contains several dwellings, extensions, yards and outbuildings.

The logical parcel must continue to own:

- population and capacity
- construction and upgrade state
- upkeep, taxation and production/consumption
- road access
- ownership
- damage or condition
- selection and interaction
- save/load identity

Its internal structures are visual composition elements unless existing architecture requires a specific structure to be interactive.

Implement a data definition using the project’s existing data architecture. A compound definition should support:

- stable definition ID
- parcel footprint
- valid road-front directions
- social class and development stage
- child mesh references
- local position, yaw and scale
- optional material or mesh variants
- required and optional structure slots
- fences, vegetation and yard-prop groups
- pedestrian entrances and yard activity nodes
- simplified compound bounds
- deterministic variation rules
- weighting and district eligibility
- corner, straight-road and edge variants

Placement rules:

- Rotate the whole compound to face its connected road.
- Keep the principal dwelling broadly aligned with the road.
- Permit controlled, historically plausible variation for secondary buildings.
- Never apply unconstrained random rotations.
- Prevent overlaps, blocked doors and inaccessible yards.
- Preserve readable parcel and road boundaries.

Use a deterministic seed derived from the persistent parcel identity. The same settlement and save must always reconstruct the same composition.

Use the project’s established instancing and rendering approach. Avoid creating a fully ticking Actor for every shed or dwelling. Batch static visual elements where practical, keep gameplay separate from rendering and support culling and HLOD.

Add:

- editor preview and validation
- missing-asset diagnostics
- deterministic-generation tests
- save/load reconstruction tests
- road-orientation tests
- overlap and bounds validation
- safe fallback when an asset is unavailable

Do not author the final layouts in this task beyond a few test fixtures. Finish with focused tests, an appropriate full build and a concise implementation report.
Prompt 3 — Author the labour-house compounds
Using the implemented compound-parcel system and the approved labour-housing asset catalogue, create production-ready labourer residential compound definitions.

Follow Hansa’s authoritative design, architectural and historical documents. Study the supplied Anno 1800 screenshot for its urban-composition principle only: regular streets contain irregular miniature neighbourhoods. Do not reproduce its buildings or layouts directly.

Create at least twelve authored compound compositions:

- four early or low-density compounds
- four established compounds
- four dense or upgraded compounds

Within those totals, include:

- straight-road parcels
- left and right corner parcels
- narrow parcels
- wider communal-yard parcels
- a limited number of workshop-oriented parcels

Morphology rules:

- principal dwellings address the street
- secondary dwellings and sheds occupy sides or rear yards
- doors connect logically to paths
- shared yards remain usable
- fences describe believable property boundaries
- workshops have working space and access
- wells, privies and storage are placed plausibly
- fire gaps and passages reflect the documented period
- modest rotations arise from boundaries and prior construction, not visual randomness
- adjacent parcels must not repeat obviously
- roofs, materials and clutter remain restrained and cohesive

Development should be visible:

Stage 1:
One or two modest dwellings, open earth yard and minimal outbuildings.

Stage 2:
Additional dwelling or workshop, more storage, fences, paths and active yard elements.

Stage 3:
Denser frontage, enlarged or upper-storey housing, more developed surfaces and reduced open space.

Upgrading must preserve the parcel identity and deterministic seed. Where possible, retain recognisable structures between stages so the neighbourhood appears to evolve rather than being replaced wholesale.

Add pedestrian entrance, yard-work, resting and delivery nodes. Validate that navigation paths do not cross walls, fences or occupied footprints.

Place representative compounds in an isolated preview settlement. Capture:

- overhead city-scale view
- normal gameplay view
- street-level or close inspection view
- comparison of all three development stages
- repeated blocks demonstrating that obvious tiling is avoided

Correct overlaps, implausible rotations, excessive clutter and repetition before completion.
Prompt 4 — Integrate and polish the real settlement flow
Use $polish-uat-loop.

Integrate and validate the new labourer compound parcels through Hansa’s real player-facing settlement flow.

Read the applicable AGENTS.md and all authoritative Hansa design and architecture documents. Exercise the actual workflow for:

- placing a labourer residence
- rotating or orienting it against roads
- constructing it
- selecting it
- inspecting population and needs
- upgrading through every visual stage
- saving and loading
- demolishing or replacing it
- viewing a district containing many adjacent parcels

Capture baseline and post-change evidence. Maintain a prioritized issue ledger and implement in-scope corrections.

Acceptance criteria:

- one parcel clearly reads as one controllable gameplay object
- each parcel looks like a small inhabited compound
- major dwellings face roads logically
- secondary structures show controlled organic variation
- adjoining parcels do not form conspicuous repeated rows
- layouts do not resemble random scattered miniatures
- corners and road orientation work correctly
- selection feedback covers the complete logical parcel
- residents can reach entrances and activity nodes
- upgrades visibly increase density
- save/load reproduces identical layouts
- no mesh intersections, floating structures or blocked passages
- distant districts render efficiently
- close views meet Hansa’s established asset quality
- the result remains commercially presentable

Profile a dense representative district and report Actor counts, instance counts, draw-call impact, frame-time impact and memory impact. Use the project’s established performance targets rather than inventing new thresholds.

Re-run the original placement-to-upgrade-to-save/load flow after every material fix. Do not call a finding verified without evidence from the real surface or the strongest documented safe substitute.
The [$hansamodels](C:\\Users\\Johan\\.codex\\skills\\hansamodels\\SKILL.md) prompt deliberately requires historical references, editable Blender sources, ImageGen-assisted PBR materials, three correction cycles, export verification and inspected Unreal imports—not merely generated mesh files.
