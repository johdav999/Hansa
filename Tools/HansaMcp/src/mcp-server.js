import readline from "node:readline";
import { HansaWireError } from "./protocol.js";

const LATEST_MCP_VERSION = "2025-11-25";
const SUPPORTED_MCP_VERSIONS = new Set([LATEST_MCP_VERSION, "2025-06-18", "2025-03-26", "2024-11-05"]);
const NO_ARGUMENTS = { type: "object", additionalProperties: false };
const SEMANTIC_ID = { type: "string", minLength: 3, maxLength: 128, pattern: "^[A-Za-z0-9._-]+\\.[A-Za-z0-9._-]+$" };
const SEMANTIC_ARGUMENT = {
  type: "object",
  required: ["semanticId"],
  properties: { semanticId: SEMANTIC_ID },
  additionalProperties: false,
};
const FIXTURE_ID = { type: "string", enum: ["mvp_production_chains_v1", "lubeck_grain_shortage_v1", "empty_lubeck_build_v1", "integrated_lubeck_city_v1"] };
const CITY_ID = { type: "string", pattern: "^City\\.[A-Za-z0-9]+$", maxLength: 64 };
const GOOD_ID = { type: "string", pattern: "^Good\\.[A-Za-z0-9]+$", maxLength: 64 };
const RUN_UNTIL_PREDICATE = {
  oneOf: [
    {
      type: "object",
      required: ["kind", "productionId", "minimumCompletedCycles"],
      properties: {
        kind: { const: "production.completed_cycles_at_least" },
        productionId: { type: "integer", minimum: 1 },
        minimumCompletedCycles: { type: "integer", minimum: 0 },
      },
      additionalProperties: false,
    },
    {
      type: "object",
      required: ["kind", "productionId", "blocker"],
      properties: {
        kind: { const: "production.blocker_equals" },
        productionId: { type: "integer", minimum: 1 },
        blocker: { type: "string", minLength: 1, maxLength: 64 },
      },
      additionalProperties: false,
    },
    {
      type: "object",
      required: ["kind", "cityId", "goodId", "alertType"],
      properties: { kind: { const: "market.alert_active" }, cityId: CITY_ID, goodId: GOOD_ID, alertType: { type: "string", enum: ["Shortage", "LowReserve", "Affordability"] } },
      additionalProperties: false,
    },
    {
      type: "object",
      required: ["kind", "cityId", "goodId", "minimumStockMilliUnits"],
      properties: { kind: { const: "market.stock_at_least" }, cityId: CITY_ID, goodId: GOOD_ID, minimumStockMilliUnits: { type: "integer", minimum: 0 } },
      additionalProperties: false,
    },
    {
      type: "object",
      required: ["kind", "cityId", "goodId", "maximumPriceMilliMarks"],
      properties: { kind: { const: "market.price_at_most" }, cityId: CITY_ID, goodId: GOOD_ID, maximumPriceMilliMarks: { type: "integer", minimum: 1 } },
      additionalProperties: false,
    },
    {
      type: "object",
      required: ["kind", "cityId", "goodId"],
      properties: { kind: { const: "market.reserve_recovered" }, cityId: CITY_ID, goodId: GOOD_ID },
      additionalProperties: false,
    },
    {
      type: "object",
      required: ["kind"],
      properties: { kind: { type: "string", enum: ["integrated.construction_completed", "integrated.inventory_moved", "integrated.production_completed", "integrated.population_grown", "integrated.bread_consumed"] } },
      additionalProperties: false,
    },
  ],
};

export const TOOLS = [
  {
    name: "capabilities_get",
    title: "Get Hansa capabilities",
    description: "Discover the running development game's protocol, permission ceiling, and bounded automation capabilities.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "session_start",
    title: "Start Hansa session",
    description: "Open the single authenticated automation session after capability discovery.",
    inputSchema: {
      type: "object",
      properties: {
        requestedPermission: { type: "string", enum: ["ReadOnly", "ControlledActions", "FixtureControl"], default: "ReadOnly" },
        requiredCapabilities: { type: "array", items: { type: "string", minLength: 1, maxLength: 64 }, uniqueItems: true },
      },
      additionalProperties: false,
    },
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "session_get",
    title: "Get Hansa session",
    description: "Read the sidecar's active authenticated Hansa automation session.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "session_stop",
    title: "Stop Hansa session",
    description: "Close the active authenticated Hansa automation session.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "ping",
    title: "Ping Hansa endpoint",
    description: "Check framed transport liveness without opening an automation session.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "health",
    title: "Get Hansa health",
    description: "Read authenticated health for the active Hansa automation session.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "fixture_list",
    title: "List Hansa fixtures",
    description: "List versioned headless fixtures available to the authenticated game process.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "fixture_load",
    title: "Load Hansa production fixture",
    description: "Reset and load the named actor-free MVP production fixture in a FixtureControl session.",
    inputSchema: { type: "object", required: ["fixtureId"], properties: { fixtureId: FIXTURE_ID }, additionalProperties: false },
    annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "gameplay_query",
    title: "Query Hansa gameplay state",
    description: "Read allowlisted fixture, integrated-city, construction, production, population, inventory, or causal market projections without exposing mutable state.",
    inputSchema: {
      type: "object",
      required: ["query"],
      properties: {
        query: { type: "string", enum: ["fixture.summary", "integrated.summary", "construction.list", "construction.get", "construction.cost", "production.list", "production.get", "population.cohort", "city.population", "inventory.stock", "market.price", "market.history", "market.components", "market.reserve", "market.explanation", "market.consumers", "market.producers", "market.alerts"] },
        buildingId: { type: "integer", minimum: 1 },
        buildingDefinitionId: { type: "string", pattern: "^Building\\.[A-Za-z0-9.]+$" },
        productionId: { type: "integer", minimum: 1 },
        populationCohortId: { type: "integer", minimum: 1 },
        inventoryId: { type: "integer", minimum: 1 },
        cityId: CITY_ID,
        goodId: GOOD_ID,
      },
      additionalProperties: false,
    },
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "gameplay_command",
    title: "Issue controlled Hansa gameplay command",
    description: "Issue an allowlisted production, residence-progression, or building-lifecycle command through the authoritative gateway; population, progress, stock, money and prices cannot be set directly.",
    inputSchema: {
      oneOf: [
        { type: "object", required: ["command", "productionId", "active"], properties: { command: { const: "production.set_active" }, productionId: { type: "integer", minimum: 1 }, active: { type: "boolean" } }, additionalProperties: false },
        { type: "object", required: ["command", "buildingId"], properties: { command: { const: "residence.upgrade" }, buildingId: { type: "integer", minimum: 1 } }, additionalProperties: false },
        { type: "object", required: ["command", "buildingId"], properties: { command: { type: "string", enum: ["construction.cancel", "building.remove"] }, buildingId: { type: "integer", minimum: 1 } }, additionalProperties: false },
      ],
    },
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "gameplay_assert",
    title: "Assert Hansa gameplay predicate",
    description: "Evaluate an allowlisted production or market predicate without advancing the simulation.",
    inputSchema: { type: "object", required: ["predicate"], properties: { predicate: RUN_UNTIL_PREDICATE }, additionalProperties: false },
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "simulation_step",
    title: "Step Hansa simulation",
    description: "Advance the loaded fixture exactly one fixed tick through the gameplay command gateway.",
    inputSchema: NO_ARGUMENTS,
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "simulation_run",
    title: "Run Hansa simulation",
    description: "Advance the loaded fixture by a bounded exact tick count through the gameplay command gateway.",
    inputSchema: { type: "object", required: ["tickCount"], properties: { tickCount: { type: "integer", minimum: 1, maximum: 10000 } }, additionalProperties: false },
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "simulation_run_until",
    title: "Run Hansa simulation until",
    description: "Advance until an allowlisted production or market predicate matches or the bounded tick budget is exhausted.",
    inputSchema: { type: "object", required: ["predicate", "maximumTicks"], properties: { predicate: RUN_UNTIL_PREDICATE, maximumTicks: { type: "integer", minimum: 1, maximum: 10000 } }, additionalProperties: false },
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "ui_find",
    title: "Find Hansa UI node",
    description: "Find one native UI node by stable semantic ID without depending on its Slate or UMG class name.",
    inputSchema: SEMANTIC_ARGUMENT,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "ui_state",
    title: "Read Hansa UI state",
    description: "Read role, label, state, bounds, relationships, focus, visibility and enabled state for a semantic node.",
    inputSchema: SEMANTIC_ARGUMENT,
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "ui_activate",
    title: "Activate Hansa UI node",
    description: "Invoke the advertised activate action on a semantic node in a ControlledActions session.",
    inputSchema: SEMANTIC_ARGUMENT,
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
  {
    name: "ui_focus",
    title: "Focus Hansa UI node",
    description: "Move native keyboard/controller focus to a semantic node in a ControlledActions session.",
    inputSchema: SEMANTIC_ARGUMENT,
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "wait_for",
    title: "Wait for Hansa UI predicate",
    description: "Wait on an observable semantic boolean predicate until it matches or its bounded monotonic deadline expires; no sleeps are used.",
    inputSchema: {
      type: "object",
      required: ["semanticId", "property"],
      properties: {
        semanticId: SEMANTIC_ID,
        property: { type: "string", enum: ["exists", "visible", "enabled", "focused", "selected", "loading", "warning", "error"] },
        expected: { type: "boolean", default: true },
        timeoutMs: { type: "integer", minimum: 1, maximum: 30000, default: 5000 },
      },
      additionalProperties: false,
    },
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  },
  {
    name: "capture_screenshot",
    title: "Capture Hansa screenshot evidence",
    description: "Capture the native proof surface at exactly 1280x720 or 1920x1080 and persist an ignored evidence bundle without post-capture resizing.",
    inputSchema: {
      type: "object",
      required: ["width", "height"],
      properties: {
        width: { type: "integer", enum: [1280, 1920] },
        height: { type: "integer", enum: [720, 1080] },
        bundleId: { type: "string", minLength: 1, maxLength: 64, pattern: "^[A-Za-z0-9_-]+$" },
      },
      additionalProperties: false,
    },
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
  },
];

function jsonRpcError(id, code, message, data) {
  return { jsonrpc: "2.0", id: id ?? null, error: { code, message, ...(data === undefined ? {} : { data }) } };
}

function toolResult(value, isError = false) {
  return {
    content: [{ type: "text", text: JSON.stringify(value) }],
    structuredContent: value,
    ...(isError ? { isError: true } : {}),
  };
}

function validGameplayPredicate(predicate) {
  if (!predicate || typeof predicate !== "object" || Array.isArray(predicate)) return false;
  const keys = Object.keys(predicate);
  const marketIds = () => /^City\.[A-Za-z0-9]+$/.test(predicate.cityId ?? "") && /^Good\.[A-Za-z0-9]+$/.test(predicate.goodId ?? "");
  if (predicate.kind === "production.completed_cycles_at_least") return keys.length === 3 && Number.isInteger(predicate.productionId) && predicate.productionId > 0 && Number.isInteger(predicate.minimumCompletedCycles) && predicate.minimumCompletedCycles >= 0;
  if (predicate.kind === "production.blocker_equals") return keys.length === 3 && Number.isInteger(predicate.productionId) && predicate.productionId > 0 && typeof predicate.blocker === "string" && predicate.blocker.length >= 1 && predicate.blocker.length <= 64;
  if (predicate.kind === "market.alert_active") return keys.length === 4 && marketIds() && ["Shortage", "LowReserve", "Affordability"].includes(predicate.alertType);
  if (predicate.kind === "market.stock_at_least") return keys.length === 4 && marketIds() && Number.isInteger(predicate.minimumStockMilliUnits) && predicate.minimumStockMilliUnits >= 0;
  if (predicate.kind === "market.price_at_most") return keys.length === 4 && marketIds() && Number.isInteger(predicate.maximumPriceMilliMarks) && predicate.maximumPriceMilliMarks > 0;
  if (["integrated.construction_completed", "integrated.inventory_moved", "integrated.production_completed", "integrated.population_grown", "integrated.bread_consumed"].includes(predicate.kind)) return keys.length === 1;
  return predicate.kind === "market.reserve_recovered" && keys.length === 3 && marketIds();
}

function validToolArguments(name, args) {
  if (!args || typeof args !== "object" || Array.isArray(args)) return false;
  const keys = Object.keys(args);
  if (["ui_find", "ui_state", "ui_activate", "ui_focus"].includes(name)) {
    return keys.length === 1 && typeof args.semanticId === "string" && /^[A-Za-z0-9._-]+\.[A-Za-z0-9._-]+$/.test(args.semanticId) && args.semanticId.length <= 128;
  }
  if (name === "wait_for") {
    if (keys.some((key) => !["semanticId", "property", "expected", "timeoutMs"].includes(key))) return false;
    return typeof args.semanticId === "string" && /^[A-Za-z0-9._-]+\.[A-Za-z0-9._-]+$/.test(args.semanticId) &&
      ["exists", "visible", "enabled", "focused", "selected", "loading", "warning", "error"].includes(args.property) &&
      (args.expected === undefined || typeof args.expected === "boolean") &&
      (args.timeoutMs === undefined || (Number.isInteger(args.timeoutMs) && args.timeoutMs >= 1 && args.timeoutMs <= 30000));
  }
  if (name === "capture_screenshot") {
    if (keys.some((key) => !["width", "height", "bundleId"].includes(key))) return false;
    const supported = (args.width === 1280 && args.height === 720) || (args.width === 1920 && args.height === 1080);
    return supported && (args.bundleId === undefined || /^[A-Za-z0-9_-]{1,64}$/.test(args.bundleId));
  }
  if (name === "fixture_load") return keys.length === 1 && ["mvp_production_chains_v1", "lubeck_grain_shortage_v1", "empty_lubeck_build_v1", "integrated_lubeck_city_v1"].includes(args.fixtureId);
  if (name === "gameplay_query") {
    if (keys.some((key) => !["query", "buildingId", "buildingDefinitionId", "productionId", "populationCohortId", "inventoryId", "cityId", "goodId"].includes(key))) return false;
    if (["fixture.summary", "integrated.summary", "construction.list", "production.list"].includes(args.query)) return keys.length === 1;
    if (args.query === "construction.get") return keys.length === 2 && Number.isInteger(args.buildingId) && args.buildingId > 0;
    if (args.query === "construction.cost") return keys.length === 2 && /^Building\.[A-Za-z0-9.]+$/.test(args.buildingDefinitionId ?? "");
    if (args.query === "production.get") return keys.length === 2 && Number.isInteger(args.productionId) && args.productionId > 0;
    if (args.query === "population.cohort") return keys.length === 2 && Number.isInteger(args.populationCohortId) && args.populationCohortId > 0;
    if (args.query === "city.population") return keys.length === 2 && /^City\.[A-Za-z0-9]+$/.test(args.cityId ?? "");
    if (args.query === "inventory.stock") return keys.length === 3 && Number.isInteger(args.inventoryId) && args.inventoryId > 0 && /^Good\.[A-Za-z0-9]+$/.test(args.goodId ?? "");
    return ["market.price", "market.history", "market.components", "market.reserve", "market.explanation", "market.consumers", "market.producers", "market.alerts"].includes(args.query) && keys.length === 3 && /^City\.[A-Za-z0-9]+$/.test(args.cityId ?? "") && /^Good\.[A-Za-z0-9]+$/.test(args.goodId ?? "");
  }
  if (name === "gameplay_command") {
    if (args.command === "production.set_active") return keys.length === 3 && Number.isInteger(args.productionId) && args.productionId > 0 && typeof args.active === "boolean";
    return keys.length === 2 && ["residence.upgrade", "construction.cancel", "building.remove"].includes(args.command) && Number.isInteger(args.buildingId) && args.buildingId > 0;
  }
  if (name === "gameplay_assert") return keys.length === 1 && validGameplayPredicate(args.predicate);
  if (name === "simulation_run") return keys.length === 1 && Number.isInteger(args.tickCount) && args.tickCount >= 1 && args.tickCount <= 10000;
  if (name === "simulation_run_until") {
    if (keys.length !== 2 || !Number.isInteger(args.maximumTicks) || args.maximumTicks < 1 || args.maximumTicks > 10000 || !args.predicate || typeof args.predicate !== "object" || Array.isArray(args.predicate)) return false;
    return validGameplayPredicate(args.predicate);
  }
  if (name !== "session_start") return keys.length === 0;
  if (keys.some((key) => key !== "requestedPermission" && key !== "requiredCapabilities")) return false;
  if (args.requestedPermission !== undefined && !["ReadOnly", "ControlledActions", "FixtureControl"].includes(args.requestedPermission)) return false;
  return args.requiredCapabilities === undefined || (
    Array.isArray(args.requiredCapabilities) &&
    new Set(args.requiredCapabilities).size === args.requiredCapabilities.length &&
    args.requiredCapabilities.every((value) => typeof value === "string" && value.length >= 1 && value.length <= 64)
  );
}

export class HansaMcpServer {
  constructor({ client, logger }) {
    this.client = client;
    this.logger = logger;
    this.initializeAnswered = false;
    this.initialized = false;
  }

  async handle(message) {
    if (!message || message.jsonrpc !== "2.0" || typeof message.method !== "string") {
      return jsonRpcError(message?.id, -32600, "Invalid Request");
    }
    const notification = message.id === undefined;
    if (message.method === "notifications/initialized") {
      this.initialized = true;
      return null;
    }
    if (message.method === "notifications/cancelled") return null;
    if (message.method === "initialize") {
      const requestedVersion = message.params?.protocolVersion;
      const protocolVersion = SUPPORTED_MCP_VERSIONS.has(requestedVersion) ? requestedVersion : LATEST_MCP_VERSION;
      this.initializeAnswered = true;
      return {
        jsonrpc: "2.0",
        id: message.id,
        result: {
          protocolVersion,
          capabilities: { tools: { listChanged: false } },
          serverInfo: { name: "hansa-mcp", title: "Hansa Development Automation", version: "0.1.0" },
          instructions: "Call capabilities_get, then session_start before authenticated tools. Production inspection requires gameplay.query, stepping requires gameplay.command with ControlledActions, and fixture_load requires fixture.control with FixtureControl. Only named fixtures, bounded ticks, allowlisted queries, and allowlisted run-until predicates are exposed. Semantic UI actions also require ControlledActions. Never automatically retry session_start or session_stop after an ambiguous disconnect; reconcile state or restart the development game. Structured errors include retryability and remedies.",
        },
      };
    }
    if (message.method === "ping") {
      return notification ? null : { jsonrpc: "2.0", id: message.id, result: {} };
    }
    if (!this.initializeAnswered || !this.initialized) {
      return notification ? null : jsonRpcError(message.id, -32002, "Server is not initialized");
    }
    if (message.method === "tools/list") {
      return { jsonrpc: "2.0", id: message.id, result: { tools: TOOLS } };
    }
    if (message.method === "tools/call") {
      if (notification) return null;
      const name = message.params?.name;
      const args = message.params?.arguments ?? {};
      const call = {
        capabilities_get: () => this.client.capabilitiesGet(),
        session_start: () => this.client.sessionStart(args),
        session_get: () => this.client.sessionGet(),
        session_stop: () => this.client.sessionStop(),
        ping: () => this.client.ping(),
        health: () => this.client.health(),
        fixture_list: () => this.client.fixtureList(),
        fixture_load: () => this.client.fixtureLoad(args.fixtureId),
        gameplay_query: () => this.client.gameplayQuery(args.query, Object.fromEntries(Object.entries(args).filter(([key]) => key !== "query"))),
        gameplay_command: () => this.client.gameplayCommand(args.command, Object.fromEntries(Object.entries(args).filter(([key]) => key !== "command"))),
        gameplay_assert: () => this.client.gameplayAssert(args.predicate),
        simulation_step: () => this.client.simulationStep(),
        simulation_run: () => this.client.simulationRun(args.tickCount),
        simulation_run_until: () => this.client.simulationRunUntil(args),
        ui_find: () => this.client.uiFind(args.semanticId),
        ui_state: () => this.client.uiState(args.semanticId),
        ui_activate: () => this.client.uiActivate(args.semanticId),
        ui_focus: () => this.client.uiFocus(args.semanticId),
        wait_for: () => this.client.waitFor(args),
        capture_screenshot: () => this.client.captureScreenshot(args),
      }[name];
      if (!call) return jsonRpcError(message.id, -32602, `Unknown tool: ${String(name)}`);
      if (!validToolArguments(name, args)) return jsonRpcError(message.id, -32602, `Invalid arguments for tool: ${name}`);
      try {
        const value = await call();
        return { jsonrpc: "2.0", id: message.id, result: toolResult(value) };
      } catch (error) {
        const structured = error instanceof HansaWireError
          ? error.toJSON()
          : new HansaWireError("SidecarError", "The sidecar failed to complete the tool call.").toJSON();
        this.logger?.log("warn", "tool.failed", { tool: name, error: structured });
        return { jsonrpc: "2.0", id: message.id, result: toolResult({ error: structured }, true) };
      }
    }
    return notification ? null : jsonRpcError(message.id, -32601, "Method not found");
  }
}

export async function serveStdio(server, { input = process.stdin, output = process.stdout } = {}) {
  const lines = readline.createInterface({ input, crlfDelay: Infinity, terminal: false });
  let queue = Promise.resolve();
  for await (const line of lines) {
    if (!line.trim()) continue;
    queue = queue.then(async () => {
      let message;
      try {
        message = JSON.parse(line);
      } catch {
        output.write(`${JSON.stringify(jsonRpcError(null, -32700, "Parse error"))}\n`);
        return;
      }
      const response = await server.handle(message);
      if (response) output.write(`${JSON.stringify(response)}\n`);
    });
  }
  await queue;
}
