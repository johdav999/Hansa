# Lübeck tree placement — 2026-09-16

Added and saved 320 individual StaticMeshActors to the configured startup/game map `/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP` through Unreal MCP. The user explicitly requested applying the previously reviewed tree family to the currently used Lübeck map. This is a placement revision on the existing staged terrain, not a Shipping release or a terrain promotion.

The ten species/age meshes are reused with seeded yaw and uniform scale variation. Twelve groups provide mixed woodland near the founding area and six regional groves. A 60m radius around the founding focus remains clear. Hydrology rejection uses the same retained water-build.json as the native water bodies and gameplay placement classifier; trunks are at least 13m outside that geometry. Candidate slopes are at most 15 degrees. Each accepted tree was grounded using a live Unreal trace near the surveyed height, avoiding interception by previously placed crowns.

The actors are grouped under `Environment/Trees/LubeckSummer/<species>`, tagged `HansaTree.LubeckPlacement.20260916`, and carry a stable authoring label such as `LubeckTree_0001_Beech_Mature`. These labels are authoring metadata, not authoritative simulation IDs. World Partition external actor and folder packages were saved. Reload verification resolved the saved World Partition actor references and found all 320 tagged actors; all 320 saved transforms match within 0.01cm. No terrain, native water geometry, gameplay building/road definitions, or saved-game state was edited.

Species counts: {'Beech': 77, 'Birch': 63, 'Oak': 102, 'Willow': 36, 'Alder': 42}. Ages: {'Mature': 216, 'Young': 104}.

Evidence: `Docs/Images/World/LubeckTrees_20260916/verification.json` and sibling native captures. Durable placement records are under `SourceArt/Generated/Trees/LubeckSummer/v1/placement/`. Reproduction tools: `Scripts/PlaceLubeckTreesMcp.py` and `Scripts/PlaceLubeckTreesBatchMcp.py`; they resume by tags/labels and do not duplicate existing trees. The supplied map uses the existing staged mesh references, consistent with its current staged terrain status.

This adds environmental trees only. Lumber hut targeting, harvest yield, automatic construction clearing, persistence of harvested trees and wind are not implemented by this placement task. The 320-actor layout has not received a forest-scale performance acceptance test.
