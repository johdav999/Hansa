import { HansaAutomationClient } from "../src/client.js";
import { NamedPipeTransport } from "../src/transport.js";

const client = new HansaAutomationClient({
  transport: new NamedPipeTransport({ pipeName: process.env.HANSA_AUTOMATION_PIPE }),
  authenticationToken: process.env.HANSA_AUTOMATION_TOKEN,
  controllerId: "s10-p04-strategic-flow",
});

const assert = (condition, message) => { if (!condition) throw new Error(message); };
const fixtureIds = ["strategic_vertical_slice_seed_alpha_v1", "strategic_vertical_slice_seed_beta_v1"];

async function runFixture(fixtureId, capture) {
  const loaded = await client.fixtureLoad(fixtureId);
  await client.uiActivate("Strategic.Action.Build");
  const shortage = await client.simulationRunUntil({ predicate: { kind: "strategic.shortage_diagnosed" }, maximumTicks: 8 });
  await client.uiActivate("Strategic.Action.Diagnose");
  const building = await client.simulationRunUntil({ predicate: { kind: "strategic.building_completed" }, maximumTicks: 32 });
  await client.uiActivate("Strategic.Action.QueueResearch");
  await client.uiActivate("Strategic.Action.StartRoutes");
  const research = await client.simulationRunUntil({
    predicate: { kind: "strategic.research_completed", technologyId: "Technology.Commerce.MarketReports" }, maximumTicks: 16,
  });
  const ai = await client.simulationRunUntil({ predicate: { kind: "strategic.ai_progressed", minimumDecisions: 2 }, maximumTicks: 32 });
  const delivery = await client.simulationRunUntil({ predicate: { kind: "strategic.route_recovered" }, maximumTicks: 64 });
  const victory = await client.simulationRunUntil({
    predicate: { kind: "strategic.victory", victoryId: "Victory.TradeNetwork" }, maximumTicks: 64,
  });

  for (const predicate of [
    { kind: "strategic.building_placed" },
    { kind: "strategic.shortage_diagnosed" },
    { kind: "strategic.route_recovered" },
    { kind: "strategic.research_completed", technologyId: "Technology.Commerce.MarketReports" },
    { kind: "strategic.ai_progressed", minimumDecisions: 2 },
    { kind: "strategic.victory", victoryId: "Victory.TradeNetwork" },
  ]) assert((await client.gameplayAssert(predicate)).matched === true, `${fixtureId}: ${predicate.kind} must remain assertable.`);

  const market = await client.gameplayQuery("market.alerts", { cityId: "City.Lubeck", goodId: "Good.Grain" });
  const researchState = await client.gameplayQuery("research.state");
  const aiHistory = await client.gameplayQuery("ai.decision_history");
  const scenario = await client.gameplayQuery("scenario.progress");
  const evidence = await client.gameplayQuery("strategic.evidence");
  assert(evidence.causalEvents.length > 0 && evidence.aiDecisions.length >= 2,
    `${fixtureId}: causal events and AI decisions must be bundled.`);
  assert(evidence.research.appliedEffects.length > 0 && evidence.objectiveState.outcome === "Victory",
    `${fixtureId}: research effects and victory objective state must be bundled.`);
  const screenshot = capture
    ? await client.captureScreenshot({ width: 1280, height: 720, bundleId: `${fixtureId}-victory` })
    : undefined;
  return { loaded, checkpoints: { building, shortage, research, ai, delivery, victory }, market, researchState, aiHistory, scenario, evidence, screenshot };
}

await client.sessionStart({
  requestedPermission: "FixtureControl",
  requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"],
});
try {
  const results = [];
  for (const fixtureId of fixtureIds) {
    const first = await runFixture(fixtureId, true);
    const replay = await runFixture(fixtureId, false);
    assert(first.evidence.stateHash === replay.evidence.stateHash, `${fixtureId}: replay state hashes differ.`);
    assert(JSON.stringify(first.evidence.aiDecisions) === JSON.stringify(replay.evidence.aiDecisions),
      `${fixtureId}: replay AI decisions differ.`);
    assert(JSON.stringify(first.evidence.objectiveState) === JSON.stringify(replay.evidence.objectiveState),
      `${fixtureId}: replay objective state differs.`);
    results.push({ fixtureId, deterministic: true, first, replayStateHash: replay.evidence.stateHash });
  }
  process.stdout.write(`${JSON.stringify({ story: "S10-P04", results }, null, 2)}\n`);
} finally {
  await client.sessionStop();
  client.transport.close();
}
