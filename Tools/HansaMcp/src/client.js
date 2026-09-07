import { randomUUID } from "node:crypto";
import { DEFAULT_TIMEOUT_MS, HansaWireError, WIRE_SCHEMA_VERSION, assertWireResponse } from "./protocol.js";

const CONTROLLER_PATTERN = /^[A-Za-z0-9._:-]{1,64}$/;
const TOKEN_PATTERN = /^\S{16,128}$/;

export class HansaAutomationClient {
  constructor({ transport, authenticationToken, controllerId = "hansa-mcp", timeoutMs = DEFAULT_TIMEOUT_MS }) {
    this.transport = transport;
    this.authenticationToken = authenticationToken;
    this.controllerId = controllerId;
    this.timeoutMs = timeoutMs;
    this.sessionId = "";
    if (!CONTROLLER_PATTERN.test(controllerId)) {
      throw new HansaWireError("ConfigurationError", "Controller ID is invalid.");
    }
  }

  async #call(operation, payload = {}, { sessionRequired = false, timeoutMs = this.timeoutMs } = {}) {
    if (sessionRequired && !this.sessionId) {
      throw new HansaWireError("NoActiveSession", "Start a Hansa automation session before using this tool.", {
        remedy: "Call capabilities_get and session_start first.",
      });
    }
    const requestId = randomUUID();
    const response = await this.transport.request({
      schemaVersion: WIRE_SCHEMA_VERSION,
      requestId,
      operation,
      controllerId: this.controllerId,
      sessionId: this.sessionId,
      timeoutMs,
      payload,
    });
    return assertWireResponse(response, requestId);
  }

  capabilitiesGet() {
    return this.#call("capabilities_get");
  }

  ping() {
    return this.#call("ping");
  }

  async sessionStart({ requestedPermission = "ReadOnly", requiredCapabilities = ["session", "capabilities", "health", "semantic-ui", "screenshots", "wait-assertions"] } = {}) {
    if (!TOKEN_PATTERN.test(this.authenticationToken ?? "")) {
      throw new HansaWireError("ConfigurationError", "HANSA_AUTOMATION_TOKEN is missing or invalid.", {
        remedy: "Set the same 16-128 character short-lived token for Hansa and HansaMcp.",
      });
    }
    const manifest = await this.capabilitiesGet();
    const session = await this.#call("session_start", {
      protocolVersion: manifest.protocolVersion,
      authenticationToken: this.authenticationToken,
      requestedPermission,
      requiredCapabilities,
    });
    this.sessionId = session.sessionId;
    return session;
  }

  sessionGet() {
    return this.#call("session_get", {}, { sessionRequired: true });
  }

  async sessionStop() {
    const result = await this.#call("session_stop", {}, { sessionRequired: true });
    this.sessionId = "";
    return result;
  }

  health() {
    return this.#call("health", {}, { sessionRequired: true });
  }

  fixtureList() {
    return this.#call("fixture_list", {}, { sessionRequired: true });
  }

  fixtureLoad(fixtureId) {
    return this.#call("fixture_load", { fixtureId }, { sessionRequired: true });
  }

  fixtureReset() {
    return this.#call("fixture_reset", {}, { sessionRequired: true });
  }

  saveList() {
    return this.#call("save_list", {}, { sessionRequired: true });
  }

  saveCreate(slotId) {
    return this.#call("save_create", { slotId }, { sessionRequired: true });
  }

  saveLoad(slotId) {
    return this.#call("save_load", { slotId }, { sessionRequired: true });
  }

  saveWaitFor(parameters) {
    return this.#call("save_wait_for", parameters, { sessionRequired: true });
  }

  saveAssertRoundTrip() {
    return this.#call("save_assert_roundtrip", {}, { sessionRequired: true });
  }
  gameplayQuery(query, parameters = {}) {
    return this.#call("gameplay_query", { query, ...parameters }, { sessionRequired: true });
  }

  gameplayCommand(command, parameters = {}) {
    return this.#call("gameplay_command", { command, ...parameters }, { sessionRequired: true });
  }

  gameplayAssert(predicate) {
    return this.#call("gameplay_assert", { predicate }, { sessionRequired: true });
  }

  simulationStep() {
    return this.#call("simulation_step", {}, { sessionRequired: true });
  }

  simulationRun(tickCount) {
    return this.#call("simulation_run", { tickCount }, { sessionRequired: true });
  }

  simulationRunUntil({ predicate, maximumTicks }) {
    return this.#call("simulation_run_until", { predicate, maximumTicks }, { sessionRequired: true });
  }

  uiFind(semanticId) {
    return this.#call("semantic_find", { semanticId }, { sessionRequired: true });
  }

  uiState(semanticId) {
    return this.#call("semantic_state", { semanticId }, { sessionRequired: true });
  }

  uiActivate(semanticId) {
    return this.#call("semantic_activate", { semanticId }, { sessionRequired: true });
  }

  uiFocus(semanticId) {
    return this.#call("semantic_focus", { semanticId }, { sessionRequired: true });
  }

  waitFor({ semanticId, property, expected = true, timeoutMs = this.timeoutMs }) {
    return this.#call("wait_for", { semanticId, property, expected }, { sessionRequired: true, timeoutMs });
  }

  captureScreenshot({ width, height, bundleId }) {
    return this.#call("screenshot_capture", {
      width,
      height,
      ...(bundleId === undefined ? {} : { bundleId }),
    }, { sessionRequired: true });
  }

  logsGet({ maximumEntries = 256 } = {}) {
    return this.#call("logs_get", { maximumEntries }, { sessionRequired: true });
  }

  evidenceBundleCreate({ bundleId, testId, assertions = [], mcpProtocolVersion = "2025-11-25" }) {
    return this.#call("evidence_bundle_create", {
      bundleId,
      testId,
      assertions,
      mcpProtocolVersion,
    }, { sessionRequired: true });
  }
}
