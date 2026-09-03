import assert from "node:assert/strict";
import test from "node:test";
import { HansaAutomationClient } from "../src/client.js";
import { FakeHansaEndpoint, FakeInProcessTransport } from "../src/fake-endpoint.js";
import { HansaWireError } from "../src/protocol.js";

const token = "contract-token-123456";

test("fake endpoint covers capability, session, semantic inspection, waits, capture and shutdown", async () => {
  const transport = new FakeInProcessTransport(new FakeHansaEndpoint({ authenticationToken: token, now: () => 42 }));
  const client = new HansaAutomationClient({ transport, authenticationToken: token, controllerId: "contract-test" });

  assert.equal((await client.ping()).pong, true);
  assert.deepEqual((await client.capabilitiesGet()).capabilities.map(({ name }) => name), ["session", "capabilities", "health", "gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"]);
  const opened = await client.sessionStart();
  assert.equal(opened.controllerId, "contract-test");
  assert.equal(opened.openedAtMonotonicMs, 42);
  assert.equal((await client.sessionGet()).sessionId, opened.sessionId);
  assert.equal((await client.health()).status, "healthy");
  assert.equal((await client.uiFind("AutomationProof.Warning")).node.role, "alert");
  assert.equal((await client.uiState("AutomationProof.Warning")).node.state.warning, true);
  assert.equal((await client.waitFor({ semanticId: "AutomationProof.Warning", property: "warning" })).matched, true);
  const capture720 = await client.captureScreenshot({ width: 1280, height: 720, bundleId: "contract-720" });
  assert.deepEqual([capture720.width, capture720.height, capture720.postCaptureResized], [1280, 720, false]);
  const capture1080 = await client.captureScreenshot({ width: 1920, height: 1080, bundleId: "contract-1080" });
  assert.deepEqual([capture1080.width, capture1080.height, capture1080.postCaptureResized], [1920, 1080, false]);
  assert.deepEqual(await client.sessionStop(), { closed: true });
  await assert.rejects(() => client.sessionGet(), (error) => error instanceof HansaWireError && error.code === "NoActiveSession");
});

test("production fixture requires explicit capabilities and advances deterministically", async () => {
  const endpoint = new FakeHansaEndpoint({ authenticationToken: token });
  const client = new HansaAutomationClient({ transport: new FakeInProcessTransport(endpoint), authenticationToken: token });
  await client.sessionStart({ requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control"] });
  assert.equal((await client.fixtureList()).fixtures[0].fixtureId, "mvp_production_chains_v1");
  assert.equal((await client.fixtureLoad("mvp_production_chains_v1")).tick, 0);
  assert.equal((await client.simulationStep()).tick, 1);
  assert.equal((await client.simulationRun(2)).tick, 3);
  const until = await client.simulationRunUntil({ maximumTicks: 10, predicate: { kind: "production.completed_cycles_at_least", productionId: 1, minimumCompletedCycles: 2 } });
  assert.equal(until.matched, true);
  assert.equal((await client.gameplayQuery("production.get", { productionId: 1 })).production.completedCycles, "2");
});

test("empty Lübeck placement flow proves invalid then valid authoritative construction", async () => {
  const endpoint = new FakeHansaEndpoint({ authenticationToken: token });
  const client = new HansaAutomationClient({ transport: new FakeInProcessTransport(endpoint), authenticationToken: token });
  await client.sessionStart({ requestedPermission: "FixtureControl", requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"] });
  const fixtures = await client.fixtureList();
  assert.ok(fixtures.fixtures.some(({ fixtureId }) => fixtureId === "empty_lubeck_build_v1"));
  assert.equal((await client.fixtureLoad("empty_lubeck_build_v1")).placedBuildingCount, 0);

  await client.uiActivate("BuildMode.Tool.Road");
  await client.uiActivate("BuildMode.Map.RoadTarget");
  await client.uiActivate("BuildMode.Action.Confirm");
  await client.uiActivate("BuildMode.Tool.Warehouse");
  await client.uiActivate("BuildMode.Map.InvalidTarget");
  const invalid = await client.uiState("BuildMode.Placement.Validation");
  assert.deepEqual([invalid.node.state.error, invalid.node.state.value], [true, "RoadRequired"]);
  await client.uiActivate("BuildMode.Map.ValidTarget");
  assert.equal((await client.uiState("BuildMode.Placement.Validation")).node.state.selected, true);
  await client.uiActivate("BuildMode.Action.Confirm");
  assert.equal((await client.waitFor({ semanticId: "BuildMode.Result.Building", property: "selected" })).matched, true);
  assert.equal((await client.uiState("BuildMode.Result.Building")).node.state.value, "Building:2:0");
  assert.match((await client.captureScreenshot({ width: 1280, height: 720, bundleId: "placement-flow" })).metadataPath, /S05P04/);
});

test("semantic actions require ControlledActions and update observable state", async () => {
  const endpoint = new FakeHansaEndpoint({ authenticationToken: token });
  const readOnly = new HansaAutomationClient({ transport: new FakeInProcessTransport(endpoint), authenticationToken: token });
  await readOnly.sessionStart();
  await assert.rejects(() => readOnly.uiActivate("AutomationProof.Activate"), (error) => error.code === "PermissionDenied");
  await readOnly.sessionStop();

  const controlled = new HansaAutomationClient({ transport: new FakeInProcessTransport(endpoint), authenticationToken: token });
  await controlled.sessionStart({ requestedPermission: "ControlledActions" });
  assert.equal((await controlled.uiActivate("AutomationProof.Activate")).node.state.selected, true);
  assert.equal((await controlled.uiFocus("AutomationProof.FocusTarget")).node.state.focused, true);
  assert.equal((await controlled.waitFor({ semanticId: "AutomationProof.FocusTarget", property: "focused" })).matched, true);
});

test("semantic and screenshot failures remain structured", async () => {
  const client = new HansaAutomationClient({ transport: new FakeInProcessTransport(new FakeHansaEndpoint({ authenticationToken: token })), authenticationToken: token });
  await client.sessionStart();
  await assert.rejects(() => client.uiState("AutomationProof.Missing"), (error) => error.code === "SemanticNodeNotFound");
  await assert.rejects(() => client.waitFor({ semanticId: "AutomationProof.Warning", property: "error", timeoutMs: 1 }), (error) => error.code === "TimedOut" && error.retryable);
  await assert.rejects(() => client.captureScreenshot({ width: 1280, height: 1080 }), (error) => error.code === "InvalidCaptureSize");
});

test("fake endpoint forwards authentication and capability errors structurally", async () => {
  const endpoint = new FakeHansaEndpoint({ authenticationToken: token });
  const wrongToken = new HansaAutomationClient({
    transport: new FakeInProcessTransport(endpoint),
    authenticationToken: "incorrect-token-1234",
  });
  await assert.rejects(() => wrongToken.sessionStart(), (error) => (
    error instanceof HansaWireError &&
    error.code === "AuthenticationFailed" &&
    error.correlationId.length > 0 &&
    error.remedy.length > 0 &&
    error.retryable === false
  ));

  const client = new HansaAutomationClient({
    transport: new FakeInProcessTransport(endpoint),
    authenticationToken: token,
  });
  await client.sessionStart({ requiredCapabilities: ["gameplay.query"] });
  await assert.rejects(() => client.fixtureLoad("mvp_production_chains_v1"), (error) => error.code === "MissingCapability");
});
