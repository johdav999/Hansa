# Route delivery fixture

`route_delivery_v1` is the S09-P04 actor-free automation fixture for the Lübeck grain-relief route. It is available only with `WITH_HANSA_AUTOMATION`; the playable runtime and Shipping package do not depend on it.

## Deterministic setup

The fixture reuses the reviewed Lübeck-shortage registry and seed. Lübeck begins below its 30,000 milli-unit grain reserve. Rostock has exportable grain, the player owns one 60,000-unit cog and one 20,000-unit wagon, and the canonical sea and land route definitions are present. The mill and brewery are paused through ordinary typed production commands during fixture setup so the route’s reserve and market effect remains independently observable; citizen demand continues normally.

Route 1 uses the cog and `Route.BalticSea`. Its simple plan unloads up to 20,000 grain in Lübeck, loads up to 20,000 in Rostock, and protects Rostock’s 30,000-unit source reserve. The fixture begins at tick 2 after setup. Direct activation departs at tick 3, arrives in Rostock at tick 13, departs after loading at tick 14, arrives in Lübeck at tick 24, and applies the delivery action at tick 25. An edit command adds one deterministic tick before that relative schedule.

The reviewed machine-readable setup is [route_delivery_v1.json](../../Tests/Fixtures/route_delivery_v1.json).

## Automation surface

The allowlisted gameplay adapter adds:

- queries: `route.list`, `route.get`, `route.cargo`, `route.events`, and `vehicle.list`;
- commands: `route.edit`, `route.set_active`, and `route.cancel`;
- run-until/assert predicates: `route.departed`, `route.arrived`, and `route.delivered`;
- semantic actions: `RouteEditor.Action.Save`, `RouteEditor.Action.Start`, `RouteEditor.Action.Cancel`, `RouteDelivery.Tab.Route`, and `RouteDelivery.Tab.Market`;
- observable statuses: `RouteDelivery.Status.Departed`, `RouteDelivery.Status.Arrived`, `RouteDelivery.Status.Delivered`, `RouteDelivery.Cargo`, and `RouteDelivery.Market.State`.

`route.delivered` deliberately accepts a positive unload recorded as either a complete transfer or a partial/missed transfer. This retains the domain’s exact outcome classification while recognizing a reserve-limited positive unload as a real delivery.

The sidecar exposes purpose-built `route_configure_relief`, `route_set_active`, `route_cancel`, `route_wait_for_phase`, `route_query_delivery`, and `route_assert_lubeck_response` tools. They only compose the same allowlisted wire operations and never mutate stock, reserve, price, or time directly.

## Evidence and verification

`npm --prefix Tools/HansaMcp run smoke:route-delivery` executes the real named-pipe flow. After delivery it captures native 1280×720 route-editor and market views under `Saved/TestEvidence/Automation/S09P04/`. Each capture stores the semantic snapshot, gameplay query snapshot, simulation tick, UI revision, state hash, ordered event count, and structural assertions together.

Focused native tests cover exact departure/arrival/delivery timing, in-transit cancellation with cargo preservation, Rostock reserve protection, remote stale-report handling, and synchronized evidence persistence. The dependency-free sidecar contract mirrors the same branches with its fake endpoint.

## Migration rule

The stable fixture ID, timing, source reserve, vehicle identities, route IDs, semantic IDs, or evidence fields must not change in place. Any behavioral change requires a new fixture version/ID and updated native and sidecar contract evidence. Generated evidence remains ignored build output; only the reviewed fixture definition and implementation documentation are checked in.
