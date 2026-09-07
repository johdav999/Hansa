const TEST_ID = "s14-p01-mvp-golden";
const FIXTURE_ID = "lubeck_grain_shortage_v1";
const REQUIRED_CAPABILITIES = [
  "session", "capabilities", "gameplay.query", "gameplay.command", "fixture.control",
  "semantic-ui", "screenshots", "wait-assertions", "evidence",
];

function invariant(condition, checkpoint, expected, actual) {
  if (condition) return;
  const error = new Error(`${checkpoint}: expected ${expected}; received ${actual}`);
  error.checkpoint = checkpoint;
  throw error;
}

export async function runGoldenMvpFlow(client, {
  bundleId = "s14-p01-mvp-golden",
  captureScreenshots = true,
  mcpProtocolVersion = "2025-11-25",
} = {}) {
  const assertions = [];
  const record = (id, passed, expected, actual) => {
    assertions.push({ id, passed });
    invariant(passed, id, expected, actual);
  };
  let phase = "capability-negotiation";
  let sessionStarted = false;
  try {
    const capabilities = await client.capabilitiesGet();
    const available = new Set(capabilities.capabilities.map(({ name }) => name));
    for (const capability of REQUIRED_CAPABILITIES) {
      record(`capability.${capability}`, available.has(capability), "available", "missing");
    }
    const session = await client.sessionStart({
      requestedPermission: "FixtureControl",
      requiredCapabilities: REQUIRED_CAPABILITIES,
    });
    sessionStarted = true;

    phase = "fixture-load-reset";
    const fixtureList = await client.fixtureList();
    const descriptor = fixtureList.fixtures.find(({ fixtureId }) => fixtureId === FIXTURE_ID);
    record("fixture.listed", Boolean(descriptor), FIXTURE_ID, "not listed");
    const loaded = await client.fixtureLoad(FIXTURE_ID);
    record("fixture.loaded", loaded.fixtureId === FIXTURE_ID, FIXTURE_ID, loaded.fixtureId);
    const initial = await client.gameplayQuery("strategic.evidence");
    const reset = await client.fixtureReset();
    const resetState = await client.gameplayQuery("strategic.evidence");
    record("fixture.reset-state-hash", resetState.stateHash === initial.stateHash, initial.stateHash, resetState.stateHash);

    phase = "semantic-shortage-diagnosis";
    const grainNode = await client.uiFind("Market.Good.Grain");
    record("semantic.market-grain-role", grainNode.node.role === "status", "status", grainNode.node.role);
    const grainState = await client.uiState("Market.Good.Grain");
    record("semantic.market-grain-warning", grainState.node.state.warning === true, true, grainState.node.state.warning);
    await client.uiActivate("Market.Action.DiagnoseGrain");
    await client.waitFor({ semanticId: "Market.Status.GrainDiagnosed", property: "selected" });
    const diagnosis = await client.gameplayQuery("market.diagnosis", { cityId: "City.Lubeck", goodId: "Good.Grain" });
    record("query.shortage-causes", diagnosis.shortageDiagnosed && diagnosis.stockMilliUnits < diagnosis.desiredReserveMilliUnits, "stock below reserve with causes", JSON.stringify(diagnosis));
    const marketCapture = captureScreenshots
      ? await client.captureScreenshot({ width: 1280, height: 720, bundleId: `${bundleId}-market` })
      : undefined;

    phase = "normal-player-actions";
    await client.uiActivate("BuildMenu.Action.ConfirmBreadChain");
    await client.simulationRunUntil({ predicate: { kind: "strategic.building_completed" }, maximumTicks: 32 });
    await client.uiActivate("TradeRoute.Editor.Action.StartRelief");
    await client.uiActivate("Research.Action.QueueMarketReports");
    await client.simulationRunUntil({ predicate: { kind: "strategic.ai_progressed", minimumDecisions: 2 }, maximumTicks: 32 });
    await client.simulationRunUntil({ predicate: { kind: "strategic.route_cargo_in_transit" }, maximumTicks: 32 });

    phase = "save-load-roundtrip";
    const preSave = await client.gameplayQuery("strategic.evidence");
    const saved = await client.saveCreate("manual");
    await client.saveWaitFor({ condition: "slot_exists", slotId: "manual" });
    const restored = await client.saveLoad("manual");
    await client.saveWaitFor({ condition: "roundtrip_verified" });
    const roundTrip = await client.saveAssertRoundTrip();
    record("save.authoritative-equivalent", restored.authoritativeEquivalent === true, true, restored.authoritativeEquivalent);
    record("save.projection-equivalent", restored.projectionEquivalent === true, true, restored.projectionEquivalent);
    record("save.deterministic-continuation", roundTrip.deterministicContinuation === true, true, roundTrip.deterministicContinuation);

    phase = "research-route-ai-victory";
    await client.simulationRunUntil({ predicate: { kind: "strategic.research_completed", technologyId: "Technology.Commerce.MarketReports" }, maximumTicks: 32 });
    await client.simulationRunUntil({ predicate: { kind: "strategic.route_recovered" }, maximumTicks: 96 });
    await client.simulationRunUntil({ predicate: { kind: "strategic.victory" }, maximumTicks: 128 });
    const final = await client.gameplayQuery("strategic.evidence");
    record("victory.valid", final.objectiveState?.outcome === "Victory" && Boolean(final.winningVictoryId), "authored victory", `${final.objectiveState?.outcome}:${final.winningVictoryId}`);
    record("ai.observed", final.aiDecisions?.length >= 2, "at least two decisions", final.aiDecisions?.length ?? 0);
    record("research.effect-applied", final.research?.appliedEffects?.length > 0, "applied effect", final.research?.appliedEffects?.length ?? 0);
    record("route.delivery-evidence", final.causalEvents?.some(({ type }) => type === "RouteCargoTransferred"), "cargo transfer event", "missing");

    phase = "evidence-capture";
    const captures = captureScreenshots ? [
      await client.captureScreenshot({ width: 1280, height: 720, bundleId: `${bundleId}-victory-720` }),
      await client.captureScreenshot({ width: 1920, height: 1080, bundleId: `${bundleId}-victory-1080` }),
    ] : [];
    for (const capture of [marketCapture, ...captures].filter(Boolean)) {
      record(`capture.native-${capture.width}x${capture.height}`, capture.postCaptureResized === false, false, capture.postCaptureResized);
    }
    const logs = await client.logsGet({ maximumEntries: 256 });
    const evidenceBundle = await client.evidenceBundleCreate({ bundleId, testId: TEST_ID, assertions, mcpProtocolVersion });
    record("evidence.bundle-complete", evidenceBundle.complete === true, true, evidenceBundle.complete);
    return {
      testId: TEST_ID,
      phase: "complete",
      protocols: { mcp: mcpProtocolVersion, automation: capabilities.protocolVersion },
      session,
      fixture: { descriptor, loaded, reset },
      stateHashes: { initial: initial.stateHash, reset: resetState.stateHash, preSave: preSave.stateHash, saved: saved.authoritativeHash, restored: restored.restoredAuthoritativeHash, final: final.stateHash },
      diagnosis,
      roundTrip,
      final,
      captures: [marketCapture, ...captures].filter(Boolean),
      logs,
      assertions,
      evidenceBundle,
    };
  } catch (error) {
    error.goldenFailure = { phase, checkpoint: error.checkpoint ?? phase, message: error.message };
    if (sessionStarted) {
      try {
        await client.evidenceBundleCreate({ bundleId: `${bundleId}-failure`, testId: TEST_ID, assertions, mcpProtocolVersion });
      } catch { /* Preserve the primary actionable failure. */ }
    }
    throw error;
  } finally {
    if (sessionStarted) {
      try { await client.sessionStop(); } catch { /* Preserve the primary result. */ }
    }
  }
}

export { FIXTURE_ID as GOLDEN_FIXTURE_ID, REQUIRED_CAPABILITIES as GOLDEN_REQUIRED_CAPABILITIES, TEST_ID as GOLDEN_TEST_ID };
