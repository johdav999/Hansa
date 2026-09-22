# Lübeck ambient rabbits

The user approved the r05 rabbit locomotion after visual review and explicitly requested production promotion and 5–6 wandering rabbits on 2026-09-20. This feature uses six by default.

## Runtime/editor contract

`AHansaAmbientRabbits` is a native, reflected, Blueprint-placeable presentation actor. The Lübeck game-mode composition creates one if none exists. `Population` (5–6), `TownRadius` (centimetres), and the three production asset references are editable in Details. Read-only Blueprint diagnostics expose live population, completed jumps and validation failures. This is standalone cosmetic wildlife, not an economic resource, AI citizen or replicated actor. It never submits simulation commands, changes checksums or adds save fields. No authoritative definition schema or save migration is introduced. Old saves acquire the same presentation on load. Random choices use an isolated campaign-seeded stream, never simulation RNG.

Initial positions are sampled within 60 metres of the Lübeck town start. Every candidate must be mapped land, loaded ground with a gentle slope, clear of authoritative building/compound cells (including a body margin), and clear of static/dynamic obstacles. Ground traces accept terrain only, never roofs. Missing terrain and rejected paths fail closed and retry. Building changes trigger refreshed footprint exclusions; rabbits displaced by development are hidden and relocated. Rabbits do not block construction.

Each rabbit rests for 2–6 seconds, walks 1.12–2.4 metres in complete gait cycles at the clip's authored 16 cm/s, then rests again. After three walks it attempts a 45 cm jump on a level, clear segment. Unsafe jump paths are retried. The jump uses the clip's actual root translation and a locked visual root, avoiding double displacement. Heading derives from the imported forward root vector, not assumptions about FBX axes. Simulation pause freezes wildlife; fast-forward does not speed cosmetic locomotion. Destination routes are conservative straight segments, not navigation-mesh paths around obstacles.

## Asset and promotion boundaries

Production folder: `/Game/Hansa/Animals/Rabbit`.

- `SK_Rabbit`, `SKEL_Rabbit`: approved 25-bone skeleton, unchanged names and source scale.
- `A_Rabbit_Walk`, `A_Rabbit_Jump`: separate 30 fps, one-second actions.
- `M_Rabbit` and base-colour, normal and roughness textures: local source texture reuse.
- LOD0 preserves the imported source; LOD1 targets 2.5% triangles for six-rabbit runtime use. The Blender master is never reduced or rewritten.

`Scripts/PromoteRabbitLocomotion.py` imports into a unique staging job, saves dependent packages explicitly, refuses existing production targets, moves the selected dependency closure, and records source/output hashes, engine version, dependencies and approval. Prior incomplete imports remain staged and excluded from cooking. No private model upload or provider call occurs.

Durable original animation evidence: `SourceArt/Generated/Animations/Animal.Rabbit.Locomotion/r05-locomotion/`.
Integration evidence: `Saved/GenerationJobs/rabbit-city_20260920_01/`.

## Validation and limitations

`Hansa.World.Rabbits.PromotedContract` checks shared skeleton identity, root/timing contracts and the reduced runtime LOD. `Hansa.World.Rabbits.OutdoorBehavior` exercises six spawns, land/footprint/path rejection, pause, walking, airborne motion and completed recovery over 180 presentation seconds. Build and actual results are recorded separately; this document is not a claim that tests have passed.

No multiplayer replication, breeding, catching, harvesting, player interaction or economy integration is included. No production map package is rewritten. Runtime settings are intentionally local presentation metadata, not new stable gameplay identities. Packaging acceptance still requires a real Shipping cook/exclusion audit; an editor build is not Shipping proof.

## Verified result — 2026-09-20

Editor build, clean-process package/root/material validation, both focused headless tests, and the surveyed Lübeck gameplay capture passed. Six rabbits remained present and completed 18 jumps across 180 presentation seconds without changing the simulation fingerprint. Runtime LOD1 has 47,062 triangles and 42,412 vertices. Walk and airborne 1920×1080 captures were inspected at native resolution. The hansaanim promotion checks preserved the existing canonical rig and master; no new animation generation was needed.

Durable promotion hashes, clean dependency audit, reviewer state, gameplay observations and screenshots: [r06 integration evidence](../SourceArt/Generated/Animations/Animal.Rabbit.Locomotion/r06-city-integration/README.md). The standalone behavior is verified; multiplayer, full Shipping cooking, and new contact-drift measurements on the reduced mesh are not claimed.
