# ADR-0002 — Stable definition and runtime identifiers

- Status: Accepted
- Date: 2026-09-01
- Decision owners: Hansa project

## Context

Definitions must survive asset moves and localization changes; campaign entities must remain unambiguous across simulation, saves, networking, UI, editor diagnostics, fixtures and MCP evidence. UObject pointers, asset paths, display names, array indexes and provider IDs are not durable gameplay identity.

## Decision

### Definition IDs

- Every definition owns a globally unique canonical string ID with typed C++ wrappers such as `FHansaGoodId`, `FHansaRecipeId` and `FHansaBuildingTypeId`.
- Canonical text uses ASCII dot-separated segments in the form `Domain.Name[.Variant]`, for example `Good.Grain`, `Recipe.Bread` and `Building.Bakery.Small`.
- Each segment begins with an ASCII letter and contains only ASCII letters or digits. IDs contain no whitespace, slash, asset path, localized text, provider name or mutable balance value.
- Domain prefixes are singular and registered centrally. Validation enforces canonical casing and global uniqueness.
- IDs are authored explicitly. They are never derived automatically from package path, object name, display name or row position.
- Published IDs are immutable and never reused for another meaning. A deliberate rename adds an explicit old-to-new redirect with migration/version coverage; deletion leaves a tombstone or documented unsupported-version boundary.
- Authoring assets and interchange JSON store the canonical ID. The compiled immutable registry may translate it to a compact typed index/handle, but deterministic compilation sorts canonical IDs before assigning compact values.
- Saves and durable manifests store canonical definition IDs plus the relevant registry/content hash. Network messages may use negotiated compact handles only after peers prove the same registry hash.

### Runtime entity IDs

- Runtime entities use separate typed wrappers such as `FHansaHouseId`, `FHansaBuildingId`, `FHansaVehicleId` and `FHansaRouteId`; unrelated ID types are not implicitly interchangeable.
- The authoritative server allocates IDs. Zero is invalid.
- The logical shape is an unsigned 64-bit value plus a generation/version when reuse is possible. The MVP should prefer monotonically increasing, non-reused values within a campaign; generation protects any future slot reuse.
- Runtime IDs are serialized in saves and command/event/evidence envelopes when the referenced entity must persist. Clients and automation never choose authoritative IDs for new entities.

### Ordering and external systems

- Any result-affecting iteration uses the canonical definition order or typed runtime numeric order, never `TMap`/`TSet` iteration order, load order, pointer value or Actor tick order.
- UI semantic IDs, localization keys, fixture IDs and protocol request IDs are separate namespaces. They may reference a Hansa definition/entity ID but do not replace it.
- Provider job IDs, URLs, filenames, voice IDs, model versions and generated asset paths are provenance, never definition or entity identity.

## Consequences

Positive:

- Asset moves, presentation changes and provider replacement do not break simulation identity.
- Saves, fixtures, multiplayer and evidence can detect content mismatch explicitly.
- Typed wrappers prevent accidental cross-domain lookup and mutation.
- Deterministic registry compilation is reviewable and hashable.

Costs:

- Designers cannot casually rename published IDs.
- Redirect/tombstone validation and migrations are mandatory.
- Runtime compact handles require a boundary conversion and registry-hash negotiation.

## Compliance

Definition schema tests must reject empty, noncanonical, duplicate and path-derived IDs. Registry tests must prove stable compact ordering and content hashes independent of asset discovery order. Save/migration tests must cover redirects. C++ compilation should reject mixing distinct typed ID wrappers without explicit conversion.

Convention clarification, 2026-09-01: [RepositoryConventions.md](../../Development/RepositoryConventions.md) records the initial definition-domain prefixes and keeps fixture, semantic UI, localization, protocol and provider/job identity in separate namespaces. The actual typed wrappers and central prefix registry remain owned by `S01-P01` and the later definition-base prompt.

Implementation clarification, 2026-09-02 (`S01-P01`): the primitive boundary stores validated canonical definition text in `FHansaDefinitionId`, with distinct `THansaDefinitionId<TTraits>` aliases for the registered domains. Runtime identities use distinct `THansaEntityId<TTraits>` aliases over an unsigned 64-bit value and unsigned 32-bit generation. The standalone `HPR1` primitive format serializes canonical definition text and the complete runtime value/generation pair in explicit little-endian fields. These choices do not make debug strings a save contract; incompatible binary changes require a primitive-format version change.

## Deferred

- Redirect asset/table implementation and supported save-migration window.
- Compact compiled-registry handles and registry-hash negotiation.
