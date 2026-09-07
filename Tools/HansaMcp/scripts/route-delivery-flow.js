import { HansaAutomationClient } from "../src/client.js";
import { NamedPipeTransport } from "../src/transport.js";

const client = new HansaAutomationClient({
  transport: new NamedPipeTransport({ pipeName: process.env.HANSA_AUTOMATION_PIPE }),
  authenticationToken: process.env.HANSA_AUTOMATION_TOKEN,
  controllerId: "s09-p04-route-flow",
});

await client.sessionStart({
  requestedPermission: "FixtureControl",
  requiredCapabilities: ["gameplay.query", "gameplay.command", "fixture.control", "semantic-ui", "screenshots", "wait-assertions"],
});
try {
  await client.fixtureLoad("route_delivery_v1");
  await client.uiActivate("RouteEditor.Action.Save");
  await client.uiActivate("RouteEditor.Action.Start");
  await client.simulationRunUntil({ predicate: { kind: "route.delivered", routeId: 1 }, maximumTicks: 64 });
  const cargo = await client.gameplayQuery("route.cargo", { routeId: 1 });
  const events = await client.gameplayQuery("route.events", { routeId: 1 });
  const market = await client.gameplayQuery("market.price", { cityId: "City.Lubeck", goodId: "Good.Grain" });
  await client.captureScreenshot({ width: 1280, height: 720, bundleId: "route-editor-delivered" });
  await client.uiActivate("RouteDelivery.Tab.Market");
  const marketEvidence = await client.captureScreenshot({ width: 1280, height: 720, bundleId: "market-after-delivery" });
  process.stdout.write(`${JSON.stringify({ cargo, events, market, marketEvidence }, null, 2)}\n`);
} finally {
  await client.sessionStop();
}
