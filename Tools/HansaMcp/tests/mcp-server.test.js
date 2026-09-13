import assert from "node:assert/strict";
import test from "node:test";
import { HansaAutomationClient } from "../src/client.js";
import { FakeHansaEndpoint, FakeInProcessTransport } from "../src/fake-endpoint.js";
import { HansaMcpServer } from "../src/mcp-server.js";

function request(id, method, params) {
  return { jsonrpc: "2.0", id, method, ...(params ? { params } : {}) };
}

function createServer(token = "mcp-contract-token-123") {
  const client = new HansaAutomationClient({
    transport: new FakeInProcessTransport(new FakeHansaEndpoint({ authenticationToken: token })),
    authenticationToken: token,
  });
  return new HansaMcpServer({ client });
}

test("MCP lifecycle negotiates protocol and exposes the bounded production tools", async () => {
  const server = createServer();
  const initialize = await server.handle(request(1, "initialize", {
    protocolVersion: "2025-11-25",
    capabilities: {},
    clientInfo: { name: "test", version: "1" },
  }));
  assert.equal(initialize.result.protocolVersion, "2025-11-25");
  assert.match(initialize.result.instructions, /capabilities_get/);
  assert.equal((await server.handle(request(2, "tools/list"))).error.code, -32002);
  assert.equal(await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" }), null);
  const list = await server.handle(request(3, "tools/list"));
  assert.deepEqual(list.result.tools.map(({ name }) => name), [
    "capabilities_get", "session_start", "session_get", "session_stop", "ping", "health",
    "fixture_list", "fixture_load", "fixture_reset", "save_list", "save_create", "save_load", "save_wait_for", "save_assert_roundtrip", "gameplay_query", "gameplay_command", "gameplay_assert", "route_configure_relief", "route_set_active", "route_cancel", "route_wait_for_phase", "route_query_delivery", "route_assert_lubeck_response", "simulation_step", "simulation_run", "simulation_run_until",
    "ui_find", "ui_state", "ui_activate", "ui_focus", "wait_for", "capture_screenshot", "logs_get", "evidence_bundle_create", "test_run",
  ]);
});

test("MCP test_run executes the full S14-P01 flow through public operations", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-06-18", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  const response = await server.handle(request(2, "tools/call", {
    name: "test_run",
    arguments: { testId: "s14-p01-mvp-golden", bundleId: "mcp-golden-contract", captureScreenshots: true },
  }));
  assert.equal(response.result.isError, undefined);
  const result = response.result.structuredContent;
  assert.equal(result.phase, "complete");
  assert.equal(result.protocols.mcp, "2025-06-18");
  assert.equal(result.fixture.loaded.fixtureId, "lubeck_grain_shortage_v1");
  assert.equal(result.stateHashes.initial, result.stateHashes.reset);
  assert.equal(result.final.objectiveState.outcome, "Victory");
  assert.equal(result.roundTrip.matched, true);
  assert.equal(result.evidenceBundle.complete, true);
  assert.equal(result.evidenceBundle.protocols.mcp, "2025-06-18");
  assert.deepEqual(result.captures.map(({ width, height, postCaptureResized }) => [width, height, postCaptureResized]), [
    [1280, 720, false], [1280, 720, false], [1920, 1080, false],
  ]);
});

test("MCP test_run preserves the failing golden phase and checkpoint", async () => {
  const server = new HansaMcpServer({
    client: { capabilitiesGet: async () => ({ protocolVersion: { major: 1, minor: 0 }, capabilities: [] }) },
  });
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  const response = await server.handle(request(2, "tools/call", {
    name: "test_run",
    arguments: { testId: "s14-p01-mvp-golden", captureScreenshots: false },
  }));
  assert.equal(response.result.isError, true);
  assert.equal(response.result.structuredContent.error.code, "GoldenTestFailed");
  assert.equal(response.result.structuredContent.error.failure.phase, "capability-negotiation");
  assert.equal(response.result.structuredContent.error.failure.checkpoint, "capability.session");
});

test("MCP fixture negotiation preserves the S04 profile unless evidence is requested", async () => {
  const ordinary = createServer();
  await ordinary.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await ordinary.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await ordinary.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control"] } }));
  const ordinaryList = await ordinary.handle(request(3, "tools/call", { name: "fixture_list", arguments: {} }));
  const ordinaryDescriptor = ordinaryList.result.structuredContent.fixtures.find(({ fixtureId }) => fixtureId === "lubeck_grain_shortage_v1");
  assert.equal(ordinaryDescriptor.fixtureVersion, 1);
  const ordinaryLoad = await ordinary.handle(request(4, "tools/call", { name: "fixture_load", arguments: { fixtureId: "lubeck_grain_shortage_v1" } }));
  assert.equal(ordinaryLoad.result.structuredContent.fixtureVersion, 1);

  const golden = createServer();
  await golden.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await golden.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await golden.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "evidence"] } }));
  const goldenList = await golden.handle(request(3, "tools/call", { name: "fixture_list", arguments: {} }));
  const matching = goldenList.result.structuredContent.fixtures.filter(({ fixtureId }) => fixtureId === "lubeck_grain_shortage_v1");
  assert.equal(matching.length, 1);
  assert.deepEqual([matching[0].fixtureVersion, matching[0].registryHash], [4, "724BD5DE8DB9C292"]);
  const goldenLoad = await golden.handle(request(4, "tools/call", { name: "fixture_load", arguments: { fixtureId: "lubeck_grain_shortage_v1" } }));
  assert.equal(goldenLoad.result.structuredContent.fixtureVersion, 4);
});

test("MCP route tools configure, deliver, query, assert, and capture synchronized evidence", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "route_delivery_v1" } }));
  assert.equal((await server.handle(request(4, "tools/call", { name: "route_configure_relief", arguments: {} }))).result.structuredContent.command, "route.edit");
  await server.handle(request(5, "tools/call", { name: "route_set_active", arguments: { active: true } }));
  const delivered = await server.handle(request(6, "tools/call", { name: "route_wait_for_phase", arguments: { phase: "delivered", maximumTicks: 64 } }));
  assert.equal(delivered.result.structuredContent.matched, true);
  const queried = await server.handle(request(7, "tools/call", { name: "route_query_delivery", arguments: {} }));
  assert.ok(queried.result.structuredContent.events.events.some(({ type }) => type === "RouteCargoTransferred"));
  assert.equal(queried.result.structuredContent.summary.stateHash, queried.result.structuredContent.events.stateHash);
  const asserted = await server.handle(request(8, "tools/call", { name: "route_assert_lubeck_response", arguments: { minimumStockMilliUnits: 1, maximumPriceMilliMarks: 1100 } }));
  assert.equal(asserted.result.structuredContent.delivered.matched, true);
  await server.handle(request(9, "tools/call", { name: "ui_activate", arguments: { semanticId: "RouteDelivery.Tab.Market" } }));
  const captured = await server.handle(request(10, "tools/call", { name: "capture_screenshot", arguments: { width: 1280, height: 720, bundleId: "route-market" } }));
  assert.match(captured.result.structuredContent.metadataPath, /S09P04/);
  assert.match(captured.result.structuredContent.querySnapshotPath, /query-snapshot\.json$/);
});

test("MCP drives the Lübeck shortage through causal queries, controlled production, and assertions", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "lubeck_grain_shortage_v1" } }));
  const shortage = await server.handle(request(4, "tools/call", { name: "simulation_run_until", arguments: { maximumTicks: 10, predicate: { kind: "market.alert_active", cityId: "City.Lubeck", goodId: "Good.Grain", alertType: "Shortage" } } }));
  assert.equal(shortage.result.structuredContent.matched, true);
  const explanation = await server.handle(request(5, "tools/call", { name: "gameplay_query", arguments: { query: "market.explanation", cityId: "City.Lubeck", goodId: "Good.Grain" } }));
  assert.equal(explanation.result.structuredContent.factors[0].factor, "Scarcity");
  await server.handle(request(6, "tools/call", { name: "gameplay_command", arguments: { command: "production.set_active", productionId: 10, active: true } }));
  await server.handle(request(7, "tools/call", { name: "gameplay_command", arguments: { command: "production.set_active", productionId: 10, active: false } }));
  await server.handle(request(8, "tools/call", { name: "simulation_run", arguments: { tickCount: 3 } }));
  const recovered = await server.handle(request(9, "tools/call", { name: "gameplay_assert", arguments: { predicate: { kind: "market.reserve_recovered", cityId: "City.Lubeck", goodId: "Good.Grain" } } }));
  assert.equal(recovered.result.structuredContent.matched, true);
  const price = await server.handle(request(10, "tools/call", { name: "gameplay_query", arguments: { query: "market.price", cityId: "City.Lubeck", goodId: "Good.Grain" } }));
  assert.ok(price.result.structuredContent.market.priceMilliMarks < 1100);
});

test("MCP exposes physical market access and local road paths as typed authoritative queries", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "fixture.control", "evidence"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "lubeck_grain_shortage_v1" } }));
  const list = await server.handle(request(4, "tools/call", { name: "gameplay_query", arguments: { query: "building.list" } }));
  assert.ok(list.result.structuredContent.buildings.some(({ roadRequired, connected }) => roadRequired && connected));
  const access = await server.handle(request(5, "tools/call", { name: "gameplay_query", arguments: { query: "building.market_access", buildingId: 1 } }));
  assert.deepEqual(
    [access.result.structuredContent.building.connected, access.result.structuredContent.building.selectedMarketBuildingId, access.result.structuredContent.building.failure],
    [true, 14, "None"],
  );
  const path = await server.handle(request(6, "tools/call", { name: "gameplay_query", arguments: { query: "logistics.path", sourceInventoryId: 1, destinationInventoryId: 2 } }));
  assert.equal(path.result.structuredContent.connected, true);
  assert.ok(path.result.structuredContent.routeCells.length > 0);
  const jobs = await server.handle(request(7, "tools/call", { name: "gameplay_query", arguments: { query: "logistics.jobs" } }));
  assert.deepEqual(jobs.result.structuredContent.jobs, []);
});

test("MCP exposes aged remote reports and typed opportunity comparisons", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "lubeck_grain_shortage_v1" } }));
  const price = await server.handle(request(4, "tools/call", { name: "gameplay_query", arguments: { query: "market.known_price", cityId: "City.Rostock", goodId: "Good.Grain" } }));
  assert.deepEqual([price.result.structuredContent.informationState, price.result.structuredContent.known, price.result.structuredContent.priceMilliMarks], ["Current", true, 850]);
  await server.handle(request(5, "tools/call", { name: "simulation_run", arguments: { tickCount: 11 } }));
  const age = await server.handle(request(6, "tools/call", { name: "gameplay_query", arguments: { query: "market.report_age", cityId: "City.Rostock", goodId: "Good.Grain" } }));
  assert.deepEqual([age.result.structuredContent.informationState, age.result.structuredContent.ageTicks], ["Estimated", 11]);
  const components = await server.handle(request(7, "tools/call", { name: "gameplay_query", arguments: { query: "market.known_components", cityId: "City.Rostock", goodId: "Good.Grain" } }));
  assert.equal(components.result.structuredContent.totalDemandMilliUnits, 1000);
  const opportunity = await server.handle(request(8, "tools/call", { name: "gameplay_query", arguments: { query: "market.opportunity", sourceCityId: "City.Rostock", destinationCityId: "City.Lubeck", goodId: "Good.Grain" } }));
  assert.equal(opportunity.result.structuredContent.comparable, true);
  assert.equal(opportunity.result.structuredContent.grossMarginMilliMarks, opportunity.result.structuredContent.destinationPriceMilliMarks - 850);
});

test("MCP routes named production fixture queries and bounded advancement", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control"] } }));
  const listed = await server.handle(request(3, "tools/call", { name: "fixture_list", arguments: {} }));
  assert.equal(listed.result.structuredContent.fixtures[0].fixtureId, "mvp_production_chains_v1");
  await server.handle(request(4, "tools/call", { name: "fixture_load", arguments: { fixtureId: "mvp_production_chains_v1" } }));
  const ran = await server.handle(request(5, "tools/call", { name: "simulation_run", arguments: { tickCount: 3 } }));
  assert.equal(ran.result.structuredContent.tick, 3);
  const until = await server.handle(request(6, "tools/call", { name: "simulation_run_until", arguments: { maximumTicks: 10, predicate: { kind: "production.completed_cycles_at_least", productionId: 1, minimumCompletedCycles: 2 } } }));
  assert.equal(until.result.structuredContent.matched, true);
  const queried = await server.handle(request(7, "tools/call", { name: "gameplay_query", arguments: { query: "production.get", productionId: 1 } }));
  assert.equal(queried.result.structuredContent.production.completedCycles, "2");
});

test("MCP exposes city population, cohort needs, and guarded residence progression", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "lubeck_grain_shortage_v1" } }));
  await server.handle(request(4, "tools/call", { name: "simulation_step", arguments: {} }));
  const city = await server.handle(request(5, "tools/call", { name: "gameplay_query", arguments: { query: "city.population", cityId: "City.Lubeck" } }));
  assert.deepEqual(
    [city.result.structuredContent.totalResidents, city.result.structuredContent.trend, city.result.structuredContent.laborerWorkforceSupply],
    [12, "Stable", 7],
  );
  const cohort = await server.handle(request(6, "tools/call", { name: "gameplay_query", arguments: { query: "population.cohort", populationCohortId: 1 } }));
  assert.equal(cohort.result.structuredContent.tierId, "PopulationTier.Laborer");
  assert.ok(cohort.result.structuredContent.needs.some(({ needId, consumedLastTickMilliUnits }) => needId === "Need.Bread" && consumedLastTickMilliUnits > 0));
  const guarded = await server.handle(request(7, "tools/call", { name: "gameplay_command", arguments: { command: "residence.upgrade", buildingId: 9 } }));
  assert.equal(guarded.result.isError, true);
  assert.match(guarded.result.structuredContent.error.message, /progression command was rejected/);
});

test("MCP runs the integrated Lübeck construction-to-consumption checkpoints", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"] } }));
  const loaded = await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "integrated_lubeck_city_v1" } }));
  assert.equal(loaded.result.structuredContent.placedBuildingCount, 10);
  let id = 4;
  for (const kind of ["integrated.construction_completed", "integrated.inventory_moved", "integrated.production_completed", "integrated.bread_consumed", "integrated.population_grown"]) {
    const waited = await server.handle(request(id++, "tools/call", { name: "simulation_run_until", arguments: { maximumTicks: 512, predicate: { kind } } }));
    assert.equal(waited.result.structuredContent.matched, true);
    const asserted = await server.handle(request(id++, "tools/call", { name: "gameplay_assert", arguments: { predicate: { kind } } }));
    assert.equal(asserted.result.structuredContent.matched, true);
  }
  const summary = await server.handle(request(id++, "tools/call", { name: "gameplay_query", arguments: { query: "integrated.summary" } }));
  assert.deepEqual(
    [summary.result.structuredContent.constructionCompleted, summary.result.structuredContent.inventoryMoved,
      summary.result.structuredContent.productionCompleted, summary.result.structuredContent.populationGrown,
      summary.result.structuredContent.breadConsumed],
    [true, true, true, true, true],
  );
  assert.ok(Number(summary.result.structuredContent.completedProductionCycles) > 0);
  assert.ok(summary.result.structuredContent.residents > 6);
  assert.ok(summary.result.structuredContent.breadConsumedTotalMilliUnits > 0);
  for (const [width, height, bundleId] of [[1280, 720, "integrated-contract-720"], [1920, 1080, "integrated-contract-1080"]]) {
    const capture = await server.handle(request(id++, "tools/call", { name: "capture_screenshot", arguments: { width, height, bundleId } }));
    assert.deepEqual([capture.result.structuredContent.width, capture.result.structuredContent.height,
      capture.result.structuredContent.postCaptureResized], [width, height, false]);
    assert.match(capture.result.structuredContent.screenshotPath, /Automation\/S06P04\//);
  }
});

test("MCP exposes construction cost, progress, completion and typed removal", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "empty_lubeck_build_v1" } }));
  const cost = await server.handle(request(4, "tools/call", { name: "gameplay_query", arguments: { query: "construction.cost", buildingDefinitionId: "Building.Warehouse" } }));
  assert.deepEqual([cost.result.structuredContent.cost.affordable, cost.result.structuredContent.cost.requiredCurrencyPfennig], [true, 2500]);

  await server.handle(request(5, "tools/call", { name: "ui_activate", arguments: { semanticId: "BuildMode.Tool.Road" } }));
  await server.handle(request(6, "tools/call", { name: "ui_activate", arguments: { semanticId: "BuildMode.Map.RoadTarget" } }));
  await server.handle(request(7, "tools/call", { name: "ui_activate", arguments: { semanticId: "BuildMode.Action.Confirm" } }));
  const initial = await server.handle(request(8, "tools/call", { name: "gameplay_query", arguments: { query: "construction.get", buildingId: 1 } }));
  assert.deepEqual([initial.result.structuredContent.construction.state, initial.result.structuredContent.construction.elapsedTicks], ["UnderConstruction", 0]);
  await server.handle(request(9, "tools/call", { name: "simulation_step", arguments: {} }));
  const completed = await server.handle(request(10, "tools/call", { name: "gameplay_query", arguments: { query: "construction.get", buildingId: 1 } }));
  assert.equal(completed.result.structuredContent.construction.state, "Completed");
  const cancel = await server.handle(request(11, "tools/call", { name: "gameplay_command", arguments: { command: "construction.cancel", buildingId: 1 } }));
  assert.equal(cancel.result.isError, true);
  const removed = await server.handle(request(12, "tools/call", { name: "gameplay_command", arguments: { command: "building.remove", buildingId: 1 } }));
  assert.equal(removed.result.structuredContent.accepted, true);
});

test("MCP validates and routes semantic, wait and exact-size capture tools", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "ControlledActions" } }));
  const focused = await server.handle(request(3, "tools/call", { name: "ui_focus", arguments: { semanticId: "AutomationProof.FocusTarget" } }));
  assert.equal(focused.result.structuredContent.node.state.focused, true);
  const waited = await server.handle(request(4, "tools/call", { name: "wait_for", arguments: { semanticId: "AutomationProof.FocusTarget", property: "focused", timeoutMs: 50 } }));
  assert.equal(waited.result.structuredContent.matched, true);
  const captured = await server.handle(request(5, "tools/call", { name: "capture_screenshot", arguments: { width: 1920, height: 1080, bundleId: "mcp-contract" } }));
  assert.equal(captured.result.structuredContent.postCaptureResized, false);
  const invalid = await server.handle(request(6, "tools/call", { name: "capture_screenshot", arguments: { width: 1280, height: 1080 } }));
  assert.equal(invalid.error.code, -32602);
});

test("MCP tools return structured content and tool-level errors", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-06-18", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  const opened = await server.handle(request(2, "tools/call", { name: "session_start", arguments: {} }));
  assert.equal(opened.result.isError, undefined);
  assert.ok(opened.result.structuredContent.sessionId);
  const health = await server.handle(request(3, "tools/call", { name: "health", arguments: {} }));
  assert.equal(health.result.structuredContent.status, "healthy");
  await server.handle(request(4, "tools/call", { name: "session_stop", arguments: {} }));
  const missing = await server.handle(request(5, "tools/call", { name: "health", arguments: {} }));
  assert.equal(missing.result.isError, true);
  assert.equal(missing.result.structuredContent.error.code, "NoActiveSession");
  const malformed = await server.handle(request(6, "tools/call", { name: "ping", arguments: { unexpected: true } }));
  assert.equal(malformed.error.code, -32602);
});

test("MCP save tools prove bounded round-trip equivalence without path arguments", async () => {
  const server = createServer();
  await server.handle(request(1, "initialize", { protocolVersion: "2025-11-25", capabilities: {}, clientInfo: {} }));
  await server.handle({ jsonrpc: "2.0", method: "notifications/initialized" });
  await server.handle(request(2, "tools/call", { name: "session_start", arguments: { requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "wait-assertions"] } }));
  await server.handle(request(3, "tools/call", { name: "fixture_load", arguments: { fixtureId: "save_roundtrip_v1" } }));
  const saved = await server.handle(request(4, "tools/call", { name: "save_create", arguments: { slotId: "manual" } }));
  assert.equal(saved.result.structuredContent.compatible, true);
  assert.equal((await server.handle(request(5, "tools/call", { name: "save_wait_for", arguments: { condition: "slot_exists", slotId: "manual" } }))).result.structuredContent.matched, true);
  const loaded = await server.handle(request(6, "tools/call", { name: "save_load", arguments: { slotId: "manual" } }));
  assert.deepEqual([loaded.result.structuredContent.authoritativeEquivalent, loaded.result.structuredContent.projectionEquivalent, loaded.result.structuredContent.deterministicContinuation], [true, true, true]);
  assert.equal((await server.handle(request(7, "tools/call", { name: "save_wait_for", arguments: { condition: "roundtrip_verified" } }))).result.structuredContent.matched, true);
  const proof = await server.handle(request(8, "tools/call", { name: "save_assert_roundtrip", arguments: {} }));
  assert.equal(proof.result.structuredContent.matched, true);
  assert.deepEqual(Object.values(proof.result.structuredContent.coverage), [true, true, true, true, true, true, true]);
  assert.equal((await server.handle(request(9, "tools/call", { name: "save_create", arguments: { slotId: "manual", path: "C:/escape" } }))).error.code, -32602);
});
