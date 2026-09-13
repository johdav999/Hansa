# EMVP-P17 harbor implementation and approved promotion

User approval on 2026-09-08 promoted seven modular harbor meshes, four materials,
twelve textures and the role Blueprint to `/Game/Mesh/hansa-harbor/`. Dock revision
2 binds this production actor. Only Dock changes catalog fingerprint; catalog v6 is
`483D86D8C5549199`. No gameplay economics or placement rules changed.

Source and full acceptance ledger:
`SourceArt/Generated/Buildings/HansaHarbor_P17_20260908/README.md` and `EVALUATION.md`.
Approval/package hashes: `Evidence/P17ApprovedPromotion-20260908.json`.

Native role actor has no tick, replication, provider code or gameplay state. Three
LODs each, identity scale, no cosmetic collision/navigation, bounded repeated
instances, construction suppression and stable berth tags are tested. Projection
preserves deck Z=100 cm despite submerged piles and keeps selection at deck height.
Dotted UObject marker names failed production Blueprint loading; plain component
names plus stable semantic tags fix this, covered by four-rotation production tests.

DebugGame and Development Editor builds succeeded. Catalog lineage and the promoted
family loader pass. Final targeted automation: 18 passed, 0 failed, with four older
projection tests reporting world-context teardown warnings. This is not a warning-free
global build: compiler calling-convention warnings and existing startup GameFeatureData
configuration errors remain outside this P17 change. The broad regression run also exposed a stale P14 mesh-only
residence expectation; it now checks the approved P14 actor and tier variants.

Rostock placement is explicitly deferred to P31 by the user; isolated 4800 K warm
and 6500 K neutral preview captures are genuine 1280x720 native renders, not a claim
of actual Rostock lighting. Full P17 acceptance remains open: actual Lübeck shore/
grade/water visual validation, Cog fit (P19), directional shore-rule acceptance,
far-distance plank aliasing and Shipping verification. The visible-content manifest
therefore remains `unverified-production`.
