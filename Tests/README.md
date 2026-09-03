# Hansa Test Data

Construction lifecycle coverage lives under `Hansa.Simulation.Construction.*`. It exercises the normal placement/cancel/remove command gateway, fixed-tick completion, currency/resource conservation, structured missing-cost rejection, save-ready snapshot fields and cancellation boundaries. The `empty_lubeck_build_v1` automation fixture exposes the matching construction queries/actions; tests must not set progress, inventory or money directly after fixture initialization.

Sprint 6 integration coverage lives at `Hansa.Architecture.Automation.IntegratedLubeckCity.LongRunParity`. The versioned `integrated_lubeck_city_v1` fixture proves construction, road-connected warehouse delivery, bakery production, residence needs consumption and growth in one state, retains sticky and cumulative checkpoint evidence, then compares equivalent rendered-world and headless projections after 512 ticks.

This tree contains reviewed, non-Unreal-package fixtures and golden test inputs. C++ fixture builders belong in `Source/HansaTests/Private/Fixtures/`; generated evidence belongs under ignored `Saved/` paths. See `Docs/Development/RepositoryConventions.md`.

`Fixtures/economic_invalid_references_v1.json` enumerates canonical-but-missing good, recipe, and building upgrade references. It drives fail-closed S03-P01 registry validation tests without becoming an alternative economic content source.

S04-P01 population validation is constructed from the reviewed in-memory definition set so each test mutates exactly one missing-good, missing-need, impossible-consumption or tier-progression condition. Runtime population tests use typed compiled registries and actor-free cohort state; no JSON fixture duplicates the authoritative formulas.

S04-P02 market validation follows the same pattern for profile settings, per-good price policy and missing-good references. S04-P03 extends runtime coverage with exact explanation reconciliation, typed price/history/component/reserve/consumer/producer queries, localization-ready causes/actions and deterministic shortage/reserve/affordability alerts. Runtime market tests construct typed city/good/inventory scenarios; report history, causal formulas and alerts are not duplicated in JSON or UI code.

`Golden/mvp_production_chains_v1.json` pins the reviewed S03-P04 fixture version, registry hash, 30-tick initial/final hashes and ordered event count. The full event/projection bundle is regenerated beneath ignored `Saved/TestEvidence/Production/`.

`Golden/lubeck_grain_shortage_v1.json` pins the S04-P04 baseline, tick-5 shortage, and tick-10 recovery metrics and state hashes. The test proves population and industrial demand, controlled production activation through the normal gateway, reserve restoration, shortage clearance, falling price, repeatability, and ordered activation events. Full structured evidence is regenerated beneath ignored `Saved/TestEvidence/Market/`; fixture migration rules are in `Docs/Development/GrainShortageFixture.md`.
