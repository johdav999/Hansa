import { HansaAutomationClient } from "../src/client.js";
import { NamedPipeTransport } from "../src/transport.js";

const token = process.env.HANSA_AUTOMATION_TOKEN;
const pipeName = process.env.HANSA_AUTOMATION_PIPE;
const client = new HansaAutomationClient({
  transport: new NamedPipeTransport({ pipeName }),
  authenticationToken: token,
  controllerId: "hansa-placement-flow",
});

const assert = (condition, message) => {
  if (!condition) throw new Error(message);
};

try {
  await client.sessionStart({
    requestedPermission: "FixtureControl",
    requiredCapabilities: ["session", "capabilities", "health", "gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"],
  });
  const loaded = await client.fixtureLoad("empty_lubeck_build_v1");
  assert(loaded.placedBuildingCount === 0, "Fixture must start empty.");

  const buildMenu = await client.uiState("BuildMenu.Root");
  assert(buildMenu.node.state.visible === true, "Build menu must be visible in the empty Lübeck fixture.");
  const roadCard = await client.uiState("BuildMenu.Card.Building_Road");
  assert(roadCard.node.state.value.includes("cost=") && roadCard.node.state.value.includes("footprint="), "Road card must expose typed cost and footprint data.");
  await client.uiActivate("BuildMenu.Card.Building_Road");
  await client.uiActivate("Placement.Target.Road");
  assert((await client.uiState("Placement.Validation")).node.state.error === false, "Road preview must be valid.");
  await client.uiActivate("Placement.Action.Confirm");

  await client.uiActivate("BuildMenu.Category.Storage");
  const warehouseCard = await client.uiState("BuildMenu.Card.Building_Warehouse");
  assert(warehouseCard.node.state.value.includes("workforce=") && warehouseCard.node.state.value.includes("flow="), "Warehouse card must expose workforce and flow data.");
  await client.uiActivate("BuildMenu.Card.Building_Warehouse");
  await client.uiActivate("Placement.Target.Disconnected");
  const invalid = await client.uiState("Placement.Validation");
  assert(invalid.node.state.error === true && invalid.node.state.value === "RoadRequired", "Disconnected warehouse must report RoadRequired.");
  const cause = await client.uiState("Placement.Validation.Cause");
  const remedy = await client.uiState("Placement.Validation.Remedy");
  assert(cause.node.label.length > 0 && remedy.node.label.length > 0, "Invalid placement must expose cause and remedy text.");

  await client.uiActivate("Placement.Target.Adjacent");
  const valid = await client.uiState("Placement.Validation");
  assert(valid.node.state.selected === true && valid.node.state.error === false, "Road-adjacent warehouse must become valid.");
  await client.uiActivate("Placement.Action.Confirm");
  await client.waitFor({ semanticId: "BuildMode.Result.Building", property: "selected", timeoutMs: 2_000 });
  const result = await client.uiState("BuildMode.Result.Building");
  assert(result.node.state.value === "Building:2:0", "Authoritative warehouse entity must be Building:2:0.");

  const capture720 = await client.captureScreenshot({ width: 1280, height: 720, bundleId: "empty-lubeck-flow-720" });
  const capture1080 = await client.captureScreenshot({ width: 1920, height: 1080, bundleId: "empty-lubeck-flow-1080" });
  process.stdout.write(`${JSON.stringify({ fixtureId: loaded.fixtureId, invalidReason: invalid.node.state.value, result: result.node.state.value, captures: [capture720, capture1080] })}\n`);
  await client.sessionStop();
} finally {
  client.transport.close();
}
