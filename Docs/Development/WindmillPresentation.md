# Animated mill presentation

`Building.Mill` uses `/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated` through the building definition's **Presentation actor** field. Its dependencies are production assets under `/Game/Hansa/Core/Buildings/Windmill/`.

The projection retains stable building identity, placement rotation, proportional footprint fitting, construction placeholder, selection and teardown. The Blueprint is a child visual; its nested rotor and native movement component remain live across production/status updates. A hidden visibility-query proxy keeps clicks associated with the managed building instead of its animated children.

## Authoring and compatibility

`UHansaBuildingDefinition.PresentationActorClass` is an optional soft Actor class reference. It takes precedence over PresentationMesh. Empty fields preserve the existing mesh behavior and deterministic hash, so old assets require no destructive migration. Existing schema-driven details, JSON Schema export, reference inspection and diff/impact mechanisms include the reflected field. Metadata declares ActorClass reference, compatible migration, included serialization and AI access Never.

Validation rejects missing, abstract, deprecated, staging and developer actor classes. Assigned references participate in the deterministic definition hash. The mill seed and accepted mill data asset reference the generated `_C` class at the user-requested path. No gameplay identity or save format is derived from that path.

The Blueprint's body and rotor roots are movable for attachment to the placement projection. Its sails turn at 36 degrees/second (6 rpm) using local Y/Pitch. Production status does not currently alter speed; construction hides the complete child hierarchy. Physics and navigation remain owned by the existing placement model.

## Verification

`Hansa.UI.World.MillBlueprintPresentation` covers the requested class, nested animation, phase preservation across projection refreshes, construction hide/reveal, stable selection, teardown, hash compatibility and rejection of staging classes. Existing projection tests cover mesh-only behavior and manager rebuilding. Production dependency inspection must show no staging/developer packages reachable from the mill Blueprint. Native game evidence and the import/assignment record are saved under `Saved/IntegrationJobs/windmill_game_20260906/`.

The underlying source and visual animation evidence are preserved in `SourceArt/Generated/Buildings/HansaWindmillAnimated_20260906_04/`. The user's request to use this Blueprint for in-game mills authorizes this production integration.

### Integration status (2026-09-06)

The production assignment was successfully saved and reloaded from disk. Development Editor builds successfully; all six `Hansa.UI.World` tests and `Hansa.Architecture.Authoring.EconomicSchemaCoverage` pass. The production dependency closure contains no staging/developer assets. Durable promotion and assignment records are under `Docs/Development/Evidence/WindmillPresentation_20260906/`.

`EconomicAssetReload` confirms all 72 production definitions compile, but the strict runtime catalog pin still differs: the approved mill assignment produces `97F691C37A6A4BFF`, while the current runtime expects `724BD5DE8DB9C292`. Automatic approval review rejected changing that pin because of older-save compatibility impact. Explicit user approval has been requested; the pin remains unchanged. Live scenario verification is pending that decision, since the existing game startup validation rejects the changed catalog. No compatibility gate has been bypassed.
