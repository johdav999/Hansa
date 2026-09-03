# Integrated Lübeck gameplay fixture (S06-P04)

`integrated_lubeck_city_v1` is the bounded Sprint 6 integration proof. It uses one authoritative simulation state for both the actor-free economic run and the disposable rendered-world projection, so parity is an exact fingerprint and projection comparison rather than a second implementation.

## Canonical world

The fixture contains one Lübeck house and city, a completed seven-cell road, a completed source warehouse, an under-construction bakery, an under-construction laborer residence, warehouse/bakery/city inventories, one bakery production row, one laborer cohort, and the Lübeck bread market. All identities, placements, stocks, policies, and timing are fixed by fixture version 1.

Normal fixed phases produce the integration sequence:

1. bakery and residence construction complete;
2. the completed road connects warehouse, bakery, and the aggregate city hand-off;
3. local logistics reserves, picks up, and delivers grain from the warehouse;
4. city workforce enables the bakery to consume grain and produce bread;
5. local logistics delivers bread to the city market inventory;
6. the residence consumes bread and sustained satisfaction produces bounded growth.

No checkpoint writes construction progress, inventory, production cycles, satisfaction, or residents directly.

## MCP contract

Load the fixture by exact ID and inspect `integrated.summary`. Observable run-until and assertion predicates are:

- `integrated.construction_completed`;
- `integrated.inventory_moved`;
- `integrated.production_completed`;
- `integrated.bread_consumed`;
- `integrated.population_grown`.

Each predicate is backed by a sticky fixture checkpoint. Once construction, a completed delivery, a production cycle, bread consumption, or population growth has been observed, later ticks cannot erase that evidence when bounded history is pruned or a last-tick quantity returns to zero. `integrated.summary` reports those five checkpoint booleans together with completed deliveries/cycles, current residents, last-tick bread consumption, and cumulative bread consumption.

The live flow is:

```powershell
npm --prefix Tools/HansaMcp run smoke:integrated
```

It advances only through the normal simulation gateway, asserts every checkpoint after the completed loop, and captures native 1280×720 and 1920×1080 screenshots under the `S06P04` evidence suite. Screenshot bundles include the semantic checkpoint snapshot and structural assertions, so evidence is not pixel-only. The dependency-free fake endpoint runs the same checkpoint and capture contract in normal MCP tests; only an explicitly enabled rendered Development game produces the real native pixels.

## Verification and evidence

`Hansa.Architecture.Automation.IntegratedLubeckCity.LongRunParity` runs equivalent world and headless instances for 512 ticks. It requires construction, delivery, production, consumption, and growth to have occurred; compares sticky checkpoint totals; checks bounded logistics records; compares final fingerprints, population, production, and every building-world projection; then materializes the initial and final projections through the disposable Actor projection manager. The ten canonical placements must remain one-to-one with world Actors, and the bakery and residence must reach the ready presentation state.

Structured evidence is written under `Saved/TestEvidence/IntegratedLubeck/integrated_lubeck_city_v1/`. It records parity hashes, every sticky checkpoint, cumulative consumption, final causal counts, and the logistics bound result. Screenshot evidence uses the normal ignored `Saved` evidence directories. These outputs are development evidence and never Shipping content.
