# ADR-0007 — Reflected definition metadata is the authoring and schema contract

- Status: Accepted
- Date: 2026-09-02
- Decision owners: Hansa project

## Context

Gameplay definitions must remain usable by runtime systems, the native Authoring Studio, deterministic validation, migration tooling, automation and future generation workers without maintaining a handwritten editor form or a separate schema for every type. Independent property lists would drift and would violate the editor/game parity contract.

## Decision

- Runtime-authored definition types derive from `UHansaDefinitionBase`. The base owns canonical stable identity, schema/authored revisions, display and localization identity, organization, deprecation/replacement data and a derived deterministic content hash.
- Every concrete definition class declares stable `HansaSchemaId` and positive `HansaSchemaVersion` class metadata.
- Every reflected definition property declares `DisplayName`, `ToolTip`, `Category`, `HansaRequired`, `HansaReference`, `HansaBulkEditable`, `HansaAIAccess`, `HansaMigration`, `HansaSerialization` and `HansaValidation`. Numeric fields also declare `HansaUnit`, `HansaMin` and `HansaMax`.
- `FHansaEditorSchemaRegistry` discovers non-abstract derived classes and their inherited fields from reflection. Class and property ordering is ordinal and deterministic.
- Missing or invalid metadata is a schema error. It is never silently inferred from C++ type, asset path, display text or field name.
- Draft 2020-12 JSON Schema is exported from the same descriptors used by the generic Editor details surface. Properties and required fields are emitted in deterministic order and Hansa classifications use `x-hansa-*` annotations.
- The native Editor-only Authoring Studio uses Asset Registry discovery, a virtualized browser, standard Details editing, structured validation and the Editor transaction system. Runtime modules do not depend on this UI or the schema exporter.
- Schema identity/version changes and golden-schema updates are reviewed changes. Breaking meaning or representation requires an explicit migration in the same implementation stream.

## Consequences

Positive:

- One approved reflected field automatically appears in generic Details and exported schema.
- Metadata coverage fails early instead of allowing editor, runtime, automation and AI contracts to diverge.
- Definition schemas are inspectable, reviewable and stable enough for future sidecars and generation workers without exposing arbitrary UObject reflection.

Costs:

- Authors must classify every reflected field completely.
- Schema changes deliberately create golden-test review work.
- Specialized visualizations may add custom editor surfaces later, but those surfaces may not replace or contradict the canonical reflected contract.

## Compliance

Automation must prove discovery of a small reflected sample, complete metadata coverage, rejection of an intentionally incomplete field, deterministic export against a reviewed golden schema, and Editor undo/redo. Development Editor, full automation, Shipping and source/configuration exclusion gates remain required.

## Deferred

- Redirect registry and asset-backed production definition catalogues.
- Migration execution, reference graph and impact-analysis UI.
- Bulk editing, diff/review and proposal staging.
- Specialized domain editors layered over the generic foundation.
