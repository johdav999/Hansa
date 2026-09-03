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
    "fixture_list", "fixture_load", "gameplay_query", "gameplay_command", "gameplay_assert", "simulation_step", "simulation_run", "simulation_run_until",
    "ui_find", "ui_state", "ui_activate", "ui_focus", "wait_for", "capture_screenshot",
  ]);
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
