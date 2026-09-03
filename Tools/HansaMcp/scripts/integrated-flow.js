import { HansaAutomationClient } from "../src/client.js";
import { NamedPipeTransport } from "../src/transport.js";

const client = new HansaAutomationClient({
  transport: new NamedPipeTransport({ pipeName: process.env.HANSA_AUTOMATION_PIPE }),
  authenticationToken: process.env.HANSA_AUTOMATION_TOKEN,
  controllerId: "hansa-integrated-lubeck-flow",
});

const assert = (condition, message) => { if (!condition) throw new Error(message); };

try {
  await client.sessionStart({
    requestedPermission: "FixtureControl",
    requiredCapabilities: ["session", "capabilities", "gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"],
  });
  const loaded = await client.fixtureLoad("integrated_lubeck_city_v1");
  assert(loaded.placedBuildingCount === 10, "Integrated fixture must load its canonical world placements.");
  const predicates = [
    "integrated.construction_completed", "integrated.inventory_moved", "integrated.production_completed",
    "integrated.bread_consumed", "integrated.population_grown",
  ];
  const checkpoints = [];
  for (const kind of predicates) checkpoints.push(await client.simulationRunUntil({ predicate: { kind }, maximumTicks: 512 }));
  for (const kind of predicates) assert((await client.gameplayAssert({ kind })).matched === true, `${kind} must remain assertable.`);
  const summary = await client.gameplayQuery("integrated.summary");
  assert(summary.constructionCompleted && summary.inventoryMoved && summary.productionCompleted &&
    summary.populationGrown && summary.breadConsumed && summary.breadConsumedTotalMilliUnits > 0,
  "Integrated summary must retain every completed checkpoint and cumulative consumption evidence.");
  const captures = [
    await client.captureScreenshot({ width: 1280, height: 720, bundleId: "integrated-lubeck-720" }),
    await client.captureScreenshot({ width: 1920, height: 1080, bundleId: "integrated-lubeck-1080" }),
  ];
  for (const capture of captures) {
    assert(capture.postCaptureResized === false && capture.screenshotPath.includes("S06P04"),
      "Integrated screenshots must be native-size S06-P04 evidence.");
  }
  process.stdout.write(`${JSON.stringify({ fixtureId: loaded.fixtureId, checkpoints, summary, captures })}\n`);
  await client.sessionStop();
} finally {
  client.transport.close();
}
