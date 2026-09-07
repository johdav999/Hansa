import assert from "node:assert/strict";
import test from "node:test";

import { HansaAutomationClient } from "../src/client.js";
import { FakeHansaEndpoint, FakeInProcessTransport } from "../src/fake-endpoint.js";
import { runGoldenMvpFlow } from "../src/golden-flow.js";

test("golden orchestration negotiates, resets, plays, saves, wins, and bundles synchronized evidence", async () => {
  const client = new HansaAutomationClient({
    transport: new FakeInProcessTransport(new FakeHansaEndpoint()),
    authenticationToken: "hansa-test-token-1234",
    controllerId: "s14-golden-contract",
  });
  const result = await runGoldenMvpFlow(client, { bundleId: "golden-contract" });
  assert.equal(result.phase, "complete");
  assert.equal(result.protocols.automation.major, 1);
	assert.equal(result.fixture.descriptor.fixtureHash, "3E1E979BCEF285C4");
  assert.equal(result.stateHashes.initial, result.stateHashes.reset);
  assert.equal(result.diagnosis.causes[0].factor, "Scarcity");
  assert.equal(result.roundTrip.deterministicContinuation, true);
  assert.ok(result.final.aiDecisions.length >= 2);
  assert.ok(result.final.causalEvents.some(({ type }) => type === "RouteCargoTransferred"));
  assert.ok(result.final.research.appliedEffects.length > 0);
  assert.equal(result.final.objectiveState.outcome, "Victory");
  assert.equal(result.logs.truncated, false);
  assert.equal(result.evidenceBundle.complete, true);
  assert.equal(result.evidenceBundle.protocols.wireSchema, 1);
  assert.ok(result.assertions.every(({ passed }) => passed));
});
