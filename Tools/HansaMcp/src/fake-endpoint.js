import { randomUUID } from "node:crypto";
import { FrameDecoder, MAX_FRAME_BYTES, WIRE_SCHEMA_VERSION, encodeFrame } from "./protocol.js";

const CAPABILITIES = [
  { name: "session", minimumPermission: "ReadOnly", mutating: false },
  { name: "capabilities", minimumPermission: "ReadOnly", mutating: false },
  { name: "health", minimumPermission: "ReadOnly", mutating: false },
  { name: "gameplay.query", minimumPermission: "ReadOnly", mutating: false },
  { name: "gameplay.command", minimumPermission: "ControlledActions", mutating: true },
  { name: "fixture.control", minimumPermission: "FixtureControl", mutating: true },
  { name: "semantic-ui", minimumPermission: "ReadOnly", mutating: true },
  { name: "screenshots", minimumPermission: "ReadOnly", mutating: false },
  { name: "wait-assertions", minimumPermission: "ReadOnly", mutating: false },
];

const PROOF_NODES = new Map([
  ["AutomationProof.Screen", { id: "AutomationProof.Screen", role: "screen", label: "Automation Proof", parentId: "", children: ["AutomationProof.Panel"], actions: [], bounds: { x: 0, y: 0, width: 1280, height: 720 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: false, error: false } }],
  ["AutomationProof.Panel", { id: "AutomationProof.Panel", role: "panel", label: "Working panel", parentId: "AutomationProof.Screen", children: ["AutomationProof.Activate", "AutomationProof.FocusTarget", "AutomationProof.Status", "AutomationProof.Title", "AutomationProof.Warning"], actions: [], bounds: { x: 80, y: 56, width: 1120, height: 608 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: false, error: false } }],
  ["AutomationProof.Title", { id: "AutomationProof.Title", role: "heading", label: "Automation Proof", parentId: "AutomationProof.Panel", children: [], actions: [], bounds: { x: 120, y: 88, width: 1040, height: 43 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: false, error: false } }],
  ["AutomationProof.Status", { id: "AutomationProof.Status", role: "status", label: "Ready — semantic state observable", parentId: "AutomationProof.Panel", children: [], actions: [], bounds: { x: 120, y: 147, width: 1040, height: 28 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: false, error: false } }],
  ["AutomationProof.Activate", { id: "AutomationProof.Activate", role: "button", label: "Activate", parentId: "AutomationProof.Panel", children: [], actions: ["activate", "focus"], bounds: { x: 130, y: 209, width: 500, height: 44 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: false, error: false } }],
  ["AutomationProof.FocusTarget", { id: "AutomationProof.FocusTarget", role: "button", label: "Focus target", parentId: "AutomationProof.Panel", children: [], actions: ["activate", "focus"], bounds: { x: 651, y: 210, width: 498, height: 42 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: false, error: false } }],
  ["AutomationProof.Warning", { id: "AutomationProof.Warning", role: "alert", label: "Reference warning", parentId: "AutomationProof.Panel", children: [], actions: [], bounds: { x: 120, y: 287, width: 1040, height: 39 }, state: { visible: true, enabled: true, focused: false, selected: false, loading: false, warning: true, error: false } }],
]);

const semanticState = (overrides = {}) => ({
  visible: true, enabled: true, focused: false, selected: false, loading: false,
  warning: false, error: false, valueType: "", value: "", ...overrides,
});

function createPlacementNodes() {
  const nodes = [
    ["BuildMode.Screen", "screen", "Lübeck build mode", "", [], semanticState()],
    ["BuildMode.Camera", "status", "Strategy camera", "BuildMode.Screen", [], semanticState({ valueType: "strategy-camera", value: "focus=-3200,-700;yawDegrees=35;zoomDistance=6500" })],
    ["BuildMode.Map", "panel", "Lübeck placement map", "BuildMode.Screen", [], semanticState()],
    ["BuildMode.Map.RoadTarget", "button", "Road target cell 18,16", "BuildMode.Map", ["activate", "focus"], semanticState({ enabled: false, valueType: "grid-coordinate", value: "18,16" })],
    ["BuildMode.Map.InvalidTarget", "button", "Disconnected target cell 10,10", "BuildMode.Map", ["activate", "focus"], semanticState({ enabled: false, valueType: "grid-coordinate", value: "10,10" })],
    ["BuildMode.Map.ValidTarget", "button", "Road-adjacent target cell 16,16", "BuildMode.Map", ["activate", "focus"], semanticState({ enabled: false, valueType: "grid-coordinate", value: "16,16" })],
    ["BuildMode.Placement.Preview", "status", "No placement preview", "BuildMode.Map", [], semanticState({ visible: false, valueType: "placement-preview" })],
    ["BuildMode.Placement.Validation", "alert", "Choose a target cell", "BuildMode.Map", [], semanticState({ visible: false, valueType: "placement-validation", value: "AwaitingPreview" })],
    ["BuildMode.Placement.Validation.Cause", "text", "No validation cause", "BuildMode.Placement.Validation", [], semanticState({ visible: false })],
    ["BuildMode.Placement.Validation.Remedy", "text", "Choose a road or building", "BuildMode.Placement.Validation", [], semanticState({ visible: false })],
    ["BuildMode.Toolbar", "panel", "Build tools", "BuildMode.Screen", [], semanticState()],
    ["BuildMode.Tool.Road", "button", "Road", "BuildMode.Toolbar", ["activate", "focus"], semanticState({ valueType: "building-definition-id", value: "Building.Road" })],
    ["BuildMode.Tool.Warehouse", "button", "Warehouse", "BuildMode.Toolbar", ["activate", "focus"], semanticState({ valueType: "building-definition-id", value: "Building.Warehouse" })],
    ["BuildMode.Action.Rotate", "button", "Rotate", "BuildMode.Toolbar", ["activate", "focus"], semanticState({ enabled: false, valueType: "grid-rotation", value: "North" })],
    ["BuildMode.Action.Repeat", "button", "Repeat", "BuildMode.Toolbar", ["activate", "focus"], semanticState({ enabled: false, selected: true, valueType: "boolean", value: "true" })],
    ["BuildMode.Action.Confirm", "button", "Confirm", "BuildMode.Toolbar", ["activate", "focus"], semanticState({ enabled: false })],
    ["BuildMode.Action.Cancel", "button", "Cancel", "BuildMode.Toolbar", ["activate", "focus"], semanticState({ enabled: false })],
    ["BuildMode.Result.Building", "status", "No committed building", "BuildMode.Screen", [], semanticState({ visible: false, valueType: "building-entity-id" })],
    ["BuildMode.Construction.Status", "status", "No active construction", "BuildMode.Screen", [], semanticState({ visible: false, valueType: "construction-state" })],
    ["BuildMode.Construction.Cost", "status", "No selected construction cost", "BuildMode.Screen", [], semanticState({ visible: false, valueType: "construction-cost" })],
  ];
  const result = new Map(nodes.map(([id, role, label, parentId, actions, state], index) => [id, {
    id, role, label, parentId, actions, state, children: [],
    bounds: { x: (index % 4) * 220, y: Math.floor(index / 4) * 80, width: 200, height: 56 },
  }]));
  for (const node of result.values()) if (node.parentId) result.get(node.parentId)?.children.push(node.id);
  for (const node of result.values()) node.children.sort();
  return result;
}

function createIntegratedNodes() {
  const result = createPlacementNodes();
  const entries = [
    ["BuildMode.Integrated", "panel", "Integrated Lübeck city loop"],
    ["BuildMode.Integrated.Construction", "status", "Integrated construction pending"],
    ["BuildMode.Integrated.Logistics", "status", "Warehouse delivery pending"],
    ["BuildMode.Integrated.Production", "status", "Bread production pending"],
    ["BuildMode.Integrated.Population", "status", "Residence growth pending"],
    ["BuildMode.Integrated.Bread", "status", "Bread consumption pending"],
  ];
  for (const [id, role, label] of entries) result.set(id, { id, role, label, parentId: id === "BuildMode.Integrated" ? "BuildMode.Screen" : "BuildMode.Integrated", actions: [], state: semanticState(), children: [], bounds: { x: 40, y: 40, width: 300, height: 40 } });
  return result;
}

function errorResponse(requestId, code, message, remedy, retryable = false) {
  return {
    schemaVersion: WIRE_SCHEMA_VERSION,
    requestId,
    ok: false,
    error: { code, correlationId: requestId, message, remedy, retryable },
  };
}

export class FakeHansaEndpoint {
  constructor({ authenticationToken = "hansa-test-token-1234", now = () => 1_000 } = {}) {
    this.authenticationToken = authenticationToken;
    this.now = now;
    this.session = null;
    this.revision = 1;
    this.nodes = new Map([...PROOF_NODES].map(([id, node]) => [id, structuredClone(node)]));
    this.fixture = null;
  }

  handle(request) {
    const requestId = typeof request?.requestId === "string" ? request.requestId : "invalid";
    if (!request || request.schemaVersion !== WIRE_SCHEMA_VERSION || !request.operation || !Number.isInteger(request.timeoutMs)) {
      return errorResponse(requestId, "InvalidRequest", "The fake endpoint rejected an invalid wire envelope.", "Use the checked-in wire schema.");
    }
    const success = (payload) => ({ schemaVersion: WIRE_SCHEMA_VERSION, requestId, ok: true, payload });
    if (request.operation === "ping") {
      return success({ pong: true, processId: 4242, protocolVersion: { major: 1, minor: 0 } });
    }
    if (request.operation === "capabilities_get") {
      return success({ protocolVersion: { major: 1, minor: 0 }, maximumPermission: "FixtureControl", capabilities: CAPABILITIES });
    }
    if (request.operation === "session_start") {
      if (request.payload?.protocolVersion?.major !== 1 || request.payload?.protocolVersion?.minor > 0) {
        return errorResponse(requestId, "IncompatibleProtocol", "The requested protocol is incompatible.", "Discover capabilities and retry with protocol 1.0.");
      }
      if (request.payload?.authenticationToken !== this.authenticationToken) {
        return errorResponse(requestId, "AuthenticationFailed", "Session authentication failed.", "Use the matching short-lived token.");
      }
      if (!["ReadOnly", "ControlledActions", "FixtureControl"].includes(request.payload?.requestedPermission)) {
        return errorResponse(requestId, "PermissionDenied", "Requested permission exceeds the fake endpoint ceiling.", "Request ReadOnly, ControlledActions, or FixtureControl.");
      }
      if (this.session) {
        return errorResponse(requestId, "SessionAlreadyOpen", "A session is already open.", "Stop it before opening another session.");
      }
      const available = new Set(CAPABILITIES.map(({ name }) => name));
      if ((request.payload?.requiredCapabilities ?? []).some((capability) => !available.has(capability))) {
        return errorResponse(requestId, "MissingCapability", "A required capability is unavailable.", "Use capabilities_get before session_start.");
      }
      this.session = {
        sessionId: randomUUID(),
        controllerId: request.controllerId,
        protocolVersion: { major: 1, minor: 0 },
        permission: request.payload.requestedPermission,
        grantedCapabilities: [...(request.payload?.requiredCapabilities ?? [])].sort(),
        openedAtMonotonicMs: this.now(),
      };
      return success(this.session);
    }
    if (!this.session) {
      return errorResponse(requestId, "NoActiveSession", "There is no active session.", "Call session_start first.");
    }
    if (request.sessionId !== this.session.sessionId || request.controllerId !== this.session.controllerId) {
      return errorResponse(requestId, "SessionMismatch", "Session identity does not match.", "Use the active sidecar session.");
    }
    if (request.operation === "session_get") {
      return success(this.session);
    }
    if (request.operation === "health") {
      return success({ status: "healthy", sessionActive: true, processId: 4242 });
    }
    if (request.operation === "fixture_list") {
      if (!this.session.grantedCapabilities.includes("gameplay.query")) return errorResponse(requestId, "MissingCapability", "The active session lacks gameplay.query.", "Open a new session requesting gameplay.query.");
      return success({ fixtures: [
        { fixtureId: "mvp_production_chains_v1", fixtureVersion: 2, registryHash: "B0481C9F740D6C18", purpose: "Headless deterministic MVP production chains" },
        { fixtureId: "lubeck_grain_shortage_v1", fixtureVersion: 2, registryHash: "B0481C9F740D6C18", purpose: "Lubeck grain shortage onset, causal inspection, and controlled recovery" },
        { fixtureId: "empty_lubeck_build_v1", fixtureVersion: 1, registryHash: "534F35504C414345", purpose: "Empty Lübeck road and building placement semantic flow" },
        { fixtureId: "integrated_lubeck_city_v1", fixtureVersion: 1, registryHash: "5330365030344C42", purpose: "Integrated Lübeck construction, logistics, production, and population world slice" },
      ] });
    }
    if (request.operation === "fixture_load") {
      if (!this.session.grantedCapabilities.includes("fixture.control")) return errorResponse(requestId, "MissingCapability", "The active session lacks fixture.control.", "Open a FixtureControl session requesting fixture.control.");
      if (this.session.permission !== "FixtureControl") return errorResponse(requestId, "PermissionDenied", "Fixture loading requires FixtureControl.", "Open a FixtureControl session.");
      if (!["mvp_production_chains_v1", "lubeck_grain_shortage_v1", "empty_lubeck_build_v1", "integrated_lubeck_city_v1"].includes(request.payload?.fixtureId)) return errorResponse(requestId, "InvalidRequest", "Unknown fixtureId.", "Call fixture_list.");
      if (request.payload.fixtureId === "integrated_lubeck_city_v1") {
        this.nodes = createIntegratedNodes();
        this.placement = { tool: "", placedBuildingCount: 10, tick: 0, preview: "", constructions: new Map([[2, { buildingId: 2, state: "UnderConstruction", elapsedTicks: 0, totalTicks: 2 }], [3, { buildingId: 3, state: "UnderConstruction", elapsedTicks: 0, totalTicks: 3 }]]), nextBuildingId: 100 };
        this.fixture = {
          id: request.payload.fixtureId, tick: 0, eventCount: 0, cycles: new Map([[1, 0]]), recoveryActive: false, market: null,
          integrated: {
            constructionCompleted: false, inventoryMoved: false, productionCompleted: false,
            populationGrown: false, breadConsumed: false, completedDeliveries: 0,
            residents: 6, breadConsumedLastTick: 0, breadConsumedTotal: 0,
          },
        };
        this.revision += 1;
        return success({ loaded: true, fixtureId: this.fixture.id, fixtureVersion: 1, tick: 0, placedBuildingCount: 10, semanticRevision: this.revision });
      }
      if (request.payload.fixtureId === "empty_lubeck_build_v1") {
        this.nodes = createPlacementNodes();
        this.placement = { tool: "", placedBuildingCount: 0, tick: 0, preview: "", constructions: new Map(), nextBuildingId: 1 };
        this.fixture = { id: request.payload.fixtureId, tick: 0, eventCount: 0, cycles: new Map(), recoveryActive: false, market: null };
        this.revision += 1;
        return success({ loaded: true, fixtureId: this.fixture.id, fixtureVersion: 1, tick: 0, placedBuildingCount: 0, semanticRevision: this.revision });
      }
      const shortage = request.payload.fixtureId === "lubeck_grain_shortage_v1";
      this.nodes = new Map([...PROOF_NODES].map(([id, node]) => [id, structuredClone(node)]));
      this.placement = null;
      this.fixture = {
        id: request.payload.fixtureId,
        tick: 0,
        eventCount: 0,
        cycles: new Map(Array.from({ length: shortage ? 4 : 9 }, (_, index) => [shortage && index === 3 ? 10 : index + 1, 0])),
        recoveryActive: false,
        market: shortage ? { stock: 16000, reserve: 30000, citizen: 0, industrial: 0, unmet: 0, price: 1000, alerts: [] } : null,
        population: shortage ? {
          cityId: "City.Lubeck", totalResidents: 12, residentChangeLastTick: 0, trend: "Stable",
          housingCapacity: 12, laborerResidents: 12, artisanResidents: 0,
          laborerWorkforceSupply: 7, laborerWorkforceAssigned: 7, laborerWorkforceAvailable: 0,
          artisanWorkforceSupply: 0, artisanWorkforceAssigned: 0, artisanWorkforceAvailable: 0,
          satisfactionBasisPoints: 10000, stapleReserveMilliDays: 0, hasMarketAccess: true,
          cohort: {
            populationCohortId: 1, residenceBuildingId: 9, cityId: "City.Lubeck",
            consumptionInventoryId: 1, tierId: "PopulationTier.Laborer", residents: 12,
            residenceCapacity: 12, residenceOperational: true, hasMarketAccess: true,
            workforceSupply: 7, accessBasisPoints: 10000, affordabilityBasisPoints: 10000,
            reliabilityBasisPoints: 10000, satisfactionBasisPoints: 10000,
            residentChangeLastTick: 0, needs: [],
          },
        } : null,
      };
      return success(this.#fixtureSummary(0));
    }
    if (request.operation === "gameplay_query") {
      if (!this.session.grantedCapabilities.includes("gameplay.query")) return errorResponse(requestId, "MissingCapability", "The active session lacks gameplay.query.", "Open a new session requesting gameplay.query.");
      if (!this.fixture) return errorResponse(requestId, "InvalidRequest", "No fixture is loaded.", "Call fixture_load first.");
      if (request.payload?.query === "fixture.summary") return success(this.#fixtureSummary(0));
      if (request.payload?.query === "integrated.summary" && this.fixture.integrated) {
        const integrated = this.fixture.integrated;
        return success({
          ...this.#fixtureSummary(0), constructionCompleted: integrated.constructionCompleted,
          inventoryMoved: integrated.inventoryMoved, productionCompleted: integrated.productionCompleted,
          populationGrown: integrated.populationGrown, breadConsumed: integrated.breadConsumed,
          completedDeliveries: integrated.completedDeliveries,
          completedProductionCycles: String(this.fixture.cycles.get(1)), residents: integrated.residents,
          breadConsumedLastTickMilliUnits: integrated.breadConsumedLastTick,
          breadConsumedTotalMilliUnits: integrated.breadConsumedTotal,
          cityBreadStockMilliUnits: this.fixture.tick >= 15 ? 1000 : 0,
        });
      }
      if (request.payload?.query === "construction.list" && this.placement) return success({ constructions: [...this.placement.constructions.values()] });
      if (request.payload?.query === "construction.get" && this.placement?.constructions.has(request.payload.buildingId)) return success({ construction: this.placement.constructions.get(request.payload.buildingId) });
      if (request.payload?.query === "construction.cost" && this.placement && ["Building.Road", "Building.Warehouse"].includes(request.payload.buildingDefinitionId)) {
        const warehouse = request.payload.buildingDefinitionId === "Building.Warehouse";
        return success({ cost: { buildingDefinitionId: request.payload.buildingDefinitionId, requiredCurrencyPfennig: warehouse ? 2500 : 25, availableCurrencyPfennig: 100000, missingCurrencyPfennig: 0, affordable: true, resources: warehouse ? [{ goodId: "Good.Planks", requiredMilliUnits: 10000, availableMilliUnits: 100000, missingMilliUnits: 0 }, { goodId: "Good.Tools", requiredMilliUnits: 2000, availableMilliUnits: 100000, missingMilliUnits: 0 }] : [{ goodId: "Good.Timber", requiredMilliUnits: 500, availableMilliUnits: 100000, missingMilliUnits: 0 }] } });
      }
      if (request.payload?.query === "production.list") return success({ productions: Array.from(this.fixture.cycles, ([productionId, completedCycles]) => ({ productionId, completedCycles: String(completedCycles), blocker: "None" })) });
      if (request.payload?.query === "production.get" && this.fixture.cycles.has(request.payload.productionId)) return success({ production: { productionId: request.payload.productionId, completedCycles: String(this.fixture.cycles.get(request.payload.productionId)), blocker: "None" } });
      if (request.payload?.query === "city.population" && this.fixture.population && request.payload.cityId === this.fixture.population.cityId) {
        const { cohort, ...city } = this.fixture.population;
        return success(city);
      }
      if (request.payload?.query === "population.cohort" && this.fixture.population?.cohort.populationCohortId === request.payload.populationCohortId) return success(structuredClone(this.fixture.population.cohort));
      if (request.payload?.query === "inventory.stock" && request.payload.inventoryId === 1) return success({ inventoryId: 1, goodId: request.payload.goodId, stockMilliUnits: request.payload.goodId === "Good.Iron" ? 60000 : 0, reservedMilliUnits: 0, availableMilliUnits: request.payload.goodId === "Good.Iron" ? 60000 : 0 });
      if (request.payload?.query?.startsWith("market.") && this.fixture.market && request.payload.cityId === "City.Lubeck" && request.payload.goodId === "Good.Grain") {
        const market = { cityId: "City.Lubeck", goodId: "Good.Grain", stockMilliUnits: this.fixture.market.stock, desiredReserveMilliUnits: this.fixture.market.reserve, citizenDemandMilliUnits: this.fixture.market.citizen, industrialDemandMilliUnits: this.fixture.market.industrial, unmetDemandMilliUnits: this.fixture.market.unmet, priceMilliMarks: this.fixture.market.price };
        if (["market.price", "market.components"].includes(request.payload.query)) return success({ market, ...(request.payload.query === "market.components" ? { factors: { scarcityBasisPoints: this.fixture.tick >= 5 ? 6000 : 0, citizenDemandBasisPoints: this.fixture.tick >= 5 ? 167 : 0, industrialDemandBasisPoints: this.fixture.tick >= 5 ? 233 : 0, unmetDemandBasisPoints: this.fixture.tick >= 5 ? 367 : 0, targetMultiplierBasisPoints: this.fixture.tick >= 5 ? 16767 : 10000 } } : {}) });
        if (request.payload.query === "market.alerts") return success({ alerts: this.fixture.market.alerts.map((type) => ({ type, severity: type === "Shortage" ? "Critical" : "Warning", activeSinceTick: 5, ageTicks: Math.max(0, this.fixture.tick - 5) })) });
        if (request.payload.query === "market.explanation") return success({ baseMultiplierBasisPoints: 10000, rawMultiplierBasisPoints: this.fixture.tick >= 5 ? 16767 : 10000, targetMultiplierBasisPoints: this.fixture.tick >= 5 ? 16767 : 10000, factors: [{ factor: "Scarcity", messageKey: "Market.Explanation.Scarcity", message: "Low stock raises the target price.", contributionBasisPoints: this.fixture.tick >= 5 ? 6000 : 0 }] });
        if (request.payload.query === "market.reserve") return success({ stockMilliUnits: this.fixture.market.stock, demandPerTickMilliUnits: 5500, reserveMilliDays: Math.floor(this.fixture.market.stock * 1000 / 5500), hasDemand: true });
        if (request.payload.query === "market.history") return success({ history: [{ tick: this.fixture.tick, stockMilliUnits: this.fixture.market.stock, citizenDemandMilliUnits: this.fixture.market.citizen, industrialDemandMilliUnits: this.fixture.market.industrial, unmetDemandMilliUnits: this.fixture.market.unmet, priceMilliMarks: this.fixture.market.price }] });
        if (request.payload.query === "market.consumers") return success({ consumers: [{ kind: "Citizen", populationCohortId: 1, demandPerTickMilliUnits: 2000 }, { kind: "Industry", productionId: 2, recipeId: "Recipe.MillFlour", demandPerTickMilliUnits: 2000 }] });
        if (request.payload.query === "market.producers") return success({ producers: [{ kind: "BackgroundSupply", productionId: 10, nominalQuantityPerCycle: 80000, active: this.fixture.recoveryActive, blocker: this.fixture.recoveryActive ? "None" : "Inactive" }] });
      }
      return errorResponse(requestId, "InvalidRequest", "Query or identifiers are not allowlisted.", "Use the documented gameplay queries.");
    }
    if (request.operation === "gameplay_command") {
      if (!this.session.grantedCapabilities.includes("gameplay.command") || this.session.permission === "ReadOnly") return errorResponse(requestId, "PermissionDenied", "Controlled gameplay commands require gameplay.command and ControlledActions.", "Open an authorized session.");
      if (this.placement && ["construction.cancel", "building.remove"].includes(request.payload?.command)) {
        const construction = this.placement.constructions.get(request.payload.buildingId);
        if (!construction || (request.payload.command === "construction.cancel" && construction.state !== "UnderConstruction") || (request.payload.command === "building.remove" && construction.state !== "Completed")) return errorResponse(requestId, "InvalidRequest", "The authoritative construction command was rejected.", "Cancel unfinished work or remove completed work.");
        this.placement.constructions.delete(request.payload.buildingId);
        this.placement.placedBuildingCount -= 1;
        this.fixture.tick += 1;
        this.placement.tick = this.fixture.tick;
        return success({ command: request.payload.command, accepted: true, tick: this.fixture.tick, placedBuildingCount: this.placement.placedBuildingCount });
      }
      if (request.payload?.command === "residence.upgrade" && this.fixture?.population) {
        const cohort = this.fixture.population.cohort;
        if (request.payload.buildingId !== cohort.residenceBuildingId || cohort.tierId !== "PopulationTier.Laborer" ||
          cohort.satisfactionBasisPoints < 8000 || cohort.residents > 8) {
          return errorResponse(requestId, "InvalidRequest", "The authoritative residence progression command was rejected.", "Meet satisfaction and target-capacity requirements before upgrading.");
        }
        cohort.tierId = "PopulationTier.Artisan";
        cohort.residenceCapacity = 8;
        cohort.workforceSupply = Math.floor(cohort.residents * 0.7);
        this.fixture.population.laborerResidents = 0;
        this.fixture.population.artisanResidents = cohort.residents;
        this.fixture.population.laborerWorkforceSupply = 0;
        this.fixture.population.artisanWorkforceSupply = cohort.workforceSupply;
        this.#advance(1);
        return success({ ...this.#fixtureSummary(1), command: request.payload.command, buildingId: request.payload.buildingId });
      }
      if (!this.fixture?.market || request.payload?.command !== "production.set_active" || request.payload?.productionId !== 10 || typeof request.payload?.active !== "boolean") return errorResponse(requestId, "InvalidRequest", "Command is not allowlisted for this fixture.", "Use production.set_active for production 10.");
      this.fixture.recoveryActive = request.payload.active;
      this.#advance(1);
      return success({ ...this.#fixtureSummary(1), command: request.payload.command, productionId: 10, active: request.payload.active });
    }
    if (request.operation === "gameplay_assert") {
      if (!this.session.grantedCapabilities.includes("wait-assertions")) return errorResponse(requestId, "MissingCapability", "The active session lacks wait-assertions.", "Request wait-assertions.");
      if (!this.fixture) return errorResponse(requestId, "InvalidRequest", "No fixture is loaded.", "Call fixture_load first.");
      const matched = this.#matches(request.payload?.predicate);
      if (matched === null) return errorResponse(requestId, "InvalidRequest", "Invalid gameplay predicate.", "Use an allowlisted predicate.");
      return success({ ...this.#fixtureSummary(0), matched, predicate: request.payload.predicate });
    }
    if (["simulation_step", "simulation_run", "simulation_run_until"].includes(request.operation)) {
      if (!this.session.grantedCapabilities.includes("gameplay.command")) return errorResponse(requestId, "MissingCapability", "The active session lacks gameplay.command.", "Open a ControlledActions session requesting gameplay.command.");
      if (this.session.permission === "ReadOnly") return errorResponse(requestId, "PermissionDenied", "Simulation advancement requires ControlledActions.", "Open a ControlledActions session.");
      if (!this.fixture) return errorResponse(requestId, "InvalidRequest", "No fixture is loaded.", "Call fixture_load first.");
      if (request.operation === "simulation_run_until") {
        const { predicate, maximumTicks } = request.payload ?? {};
        if (this.#matches(predicate) === null || !Number.isInteger(maximumTicks) || maximumTicks < 1 || maximumTicks > 10000) return errorResponse(requestId, "InvalidRequest", "Invalid run-until predicate.", "Use an allowlisted predicate and bounded maximumTicks.");
        let advanced = 0;
        while (!this.#matches(predicate) && advanced < maximumTicks) { this.#advance(1); advanced += 1; }
        return success({ ...this.#fixtureSummary(advanced), matched: this.#matches(predicate) });
      }
      const ticks = request.operation === "simulation_step" ? 1 : request.payload?.tickCount;
      if (!Number.isInteger(ticks) || ticks < 1 || ticks > 10000) return errorResponse(requestId, "InvalidRequest", "Invalid tick count.", "Use 1 through 10000.");
      this.#advance(ticks);
      return success(this.#fixtureSummary(ticks));
    }
    if (["semantic_find", "semantic_state", "semantic_activate", "semantic_focus"].includes(request.operation)) {
      if (!this.session.grantedCapabilities.includes("semantic-ui")) {
        return errorResponse(requestId, "MissingCapability", "The active session lacks semantic-ui.", "Open a new session requesting semantic-ui.");
      }
      const node = this.nodes.get(request.payload?.semanticId);
      if (!node) return errorResponse(requestId, "SemanticNodeNotFound", "The semantic node was not found.", "Use a stable ID from the proof screen.");
      if (["semantic_activate", "semantic_focus"].includes(request.operation)) {
        if (this.session.permission === "ReadOnly") {
          return errorResponse(requestId, "PermissionDenied", "The active session cannot invoke UI actions.", "Open a ControlledActions session.");
        }
        const action = request.operation === "semantic_activate" ? "activate" : "focus";
        if (!node.actions.includes(action)) return errorResponse(requestId, "SemanticActionUnsupported", "The semantic action is unsupported.", "Inspect the node actions.");
        if (action === "activate" && this.placement) {
          if (!this.#activatePlacement(request.payload.semanticId)) return errorResponse(requestId, "SemanticActionUnsupported", "The placement intent is unavailable in the current state.", "Follow the documented road then warehouse flow.");
        } else if (action === "activate") node.state.selected = !node.state.selected;
        if (action === "focus") {
          for (const value of this.nodes.values()) value.state.focused = false;
          node.state.focused = true;
          node.state.selected = true;
        }
        this.revision += 1;
      }
      return success({ revision: this.revision, node: structuredClone(node) });
    }
    if (request.operation === "wait_for") {
      if (!this.session.grantedCapabilities.includes("wait-assertions")) {
        return errorResponse(requestId, "MissingCapability", "The active session lacks wait-assertions.", "Open a new session requesting wait-assertions.");
      }
      const node = this.nodes.get(request.payload?.semanticId);
      const observed = request.payload?.property === "exists" ? Boolean(node) : node?.state?.[request.payload?.property];
      if (typeof observed === "boolean" && observed === request.payload?.expected) {
        return success({ matched: true, semanticId: request.payload.semanticId, property: request.payload.property, expected: request.payload.expected, revision: this.revision });
      }
      return errorResponse(requestId, "TimedOut", "The observable semantic predicate did not match before its deadline.", "Inspect semantic_state and correct the predicate.", true);
    }
    if (request.operation === "screenshot_capture") {
      if (!this.session.grantedCapabilities.includes("screenshots")) {
        return errorResponse(requestId, "MissingCapability", "The active session lacks screenshots.", "Open a new session requesting screenshots.");
      }
      const { width, height } = request.payload ?? {};
      if (!((width === 1280 && height === 720) || (width === 1920 && height === 1080))) {
        return errorResponse(requestId, "InvalidCaptureSize", "The requested screenshot size is unsupported.", "Request exactly 1280x720 or 1920x1080.");
      }
      const bundle = request.payload.bundleId ?? requestId;
      if (!/^[A-Za-z0-9_-]{1,64}$/.test(bundle)) {
        return errorResponse(requestId, "InvalidRequest", "The bundle ID is unsafe.", "Use 1-64 letters, digits, underscores, or hyphens.");
      }
      const suite = this.fixture?.integrated ? "S06P04" : (this.placement ? "S05P04" : "S02P04");
      return success({ width, height, postCaptureResized: false, screenshotPath: `Saved/TestEvidence/Automation/${suite}/${bundle}/screenshot-${width}x${height}.png`, metadataPath: `Saved/TestEvidence/Automation/${suite}/${bundle}/metadata.json`, semanticSnapshotPath: `Saved/TestEvidence/Automation/${suite}/${bundle}/semantic-ui.json`, contentSha1: "fake-contract-sha1", revision: this.revision });
    }
    if (request.operation === "session_stop") {
      this.session = null;
      return success({ closed: true });
    }
    return errorResponse(requestId, "InvalidRequest", "The operation is outside the Hansa automation surface.", "Use a listed tool.");
  }

  #advance(ticks) {
    for (let index = 0; index < ticks; index += 1) {
      this.fixture.tick += 1;
      if (this.placement) {
        this.placement.tick = this.fixture.tick;
        for (const construction of this.placement.constructions.values()) {
          if (construction.state !== "UnderConstruction") continue;
          construction.elapsedTicks = Math.min(construction.totalTicks, construction.elapsedTicks + 1);
          construction.progressPartsPerMillion = Math.floor(construction.elapsedTicks * 1000000 / construction.totalTicks);
          if (construction.elapsedTicks === construction.totalTicks) construction.state = "Completed";
        }
        const latest = [...this.placement.constructions.values()].at(-1);
        const status = this.nodes.get("BuildMode.Construction.Status");
        if (latest && status) {
          status.state = semanticState({ visible: true, selected: latest.state === "Completed", valueType: "construction-state", value: `building=Building:${latest.buildingId}:0;state=${latest.state};elapsedTicks=${latest.elapsedTicks};totalTicks=${latest.totalTicks};progressPpm=${latest.progressPartsPerMillion}` });
          status.label = `${latest.state}: ${latest.elapsedTicks}/${latest.totalTicks} ticks`;
        }
      }
      if (this.fixture.integrated) {
        const integrated = this.fixture.integrated;
        if (this.fixture.tick >= 3) integrated.constructionCompleted = true;
        if (this.fixture.tick >= 10) {
          integrated.completedDeliveries = 1;
          integrated.inventoryMoved = true;
        }
        if (this.fixture.tick >= 12) {
          this.fixture.cycles.set(1, this.fixture.tick - 11);
          integrated.productionCompleted = true;
        }
        integrated.breadConsumedLastTick = this.fixture.tick >= 15 ? 60 : 0;
        if (integrated.breadConsumedLastTick > 0) {
          integrated.breadConsumed = true;
          integrated.breadConsumedTotal += integrated.breadConsumedLastTick;
        }
        if (this.fixture.tick >= 20) {
          integrated.residents = 7;
          integrated.populationGrown = true;
        }
        const states = {
          "BuildMode.Integrated.Construction": this.fixture.tick >= 3,
          "BuildMode.Integrated.Logistics": this.fixture.tick >= 10,
          "BuildMode.Integrated.Production": this.fixture.tick >= 12,
          "BuildMode.Integrated.Population": this.fixture.tick >= 20,
          "BuildMode.Integrated.Bread": this.fixture.tick >= 15,
        };
        for (const [id, selected] of Object.entries(states)) if (this.nodes.has(id)) this.nodes.get(id).state.selected = selected;
      }
      if (this.fixture.market) {
        if (this.fixture.recoveryActive) this.fixture.market.stock += 80000;
        this.fixture.market.stock = Math.max(0, this.fixture.market.stock - 5500);
        if (this.fixture.tick % 5 === 0) {
          this.fixture.market.citizen = 2000;
          this.fixture.market.industrial = 3500;
          this.fixture.market.unmet = this.fixture.market.stock === 0 ? 5500 : 0;
          this.fixture.market.price = this.fixture.market.unmet > 0 ? 1100 : Math.max(500, this.fixture.market.price - 100);
          this.fixture.market.alerts = this.fixture.market.unmet > 0 ? ["Shortage", "LowReserve"] : (this.fixture.market.stock < this.fixture.market.reserve ? ["LowReserve"] : []);
        }
      }
      if (this.fixture.population) {
        const population = this.fixture.population;
        population.residentChangeLastTick = 0;
        population.trend = "Stable";
        population.cohort.residentChangeLastTick = 0;
        population.cohort.needs = [
          { needId: "Need.BasicServices", goodId: "", requiredLastTickMilliUnits: 0, consumedLastTickMilliUnits: 0, accessBasisPoints: 10000, affordabilityBasisPoints: 10000, reliabilityBasisPoints: 10000, satisfactionBasisPoints: 10000, reserveMilliDays: 0 },
          { needId: "Need.Bread", goodId: "Good.Bread", requiredLastTickMilliUnits: population.totalResidents * 100, consumedLastTickMilliUnits: population.totalResidents * 100, accessBasisPoints: 10000, affordabilityBasisPoints: 10000, reliabilityBasisPoints: 10000, satisfactionBasisPoints: 10000, reserveMilliDays: 1000 },
        ];
      }
      if (!this.fixture.integrated) for (const productionId of this.fixture.cycles.keys()) this.fixture.cycles.set(productionId, Math.floor(this.fixture.tick / (productionId === 1 || productionId === 9 ? 3 : 2)));
      this.fixture.eventCount += 1;
    }
  }

  #activatePlacement(id) {
    const get = (semanticId) => this.nodes.get(semanticId);
    const setTool = (tool) => {
      this.placement.tool = tool;
      get("BuildMode.Tool.Road").state.selected = tool === "road";
      get("BuildMode.Tool.Warehouse").state.selected = tool === "warehouse";
      get("BuildMode.Map.RoadTarget").state.enabled = tool === "road";
      get("BuildMode.Map.InvalidTarget").state.enabled = tool === "warehouse";
      get("BuildMode.Map.ValidTarget").state.enabled = tool === "warehouse";
      get("BuildMode.Action.Rotate").state.enabled = tool === "warehouse";
      get("BuildMode.Action.Repeat").state.enabled = true;
      get("BuildMode.Action.Cancel").state.enabled = true;
      get("BuildMode.Action.Confirm").state.enabled = false;
      this.placement.preview = "";
      return true;
    };
    const validate = (error, value, cause, remedy) => {
      const preview = get("BuildMode.Placement.Preview");
      const validation = get("BuildMode.Placement.Validation");
      preview.state = semanticState({ selected: true, error, valueType: "placement-preview", value: `definition=Building.Warehouse;canPlace=${!error}` });
      validation.state = semanticState({ selected: !error, error, valueType: "placement-validation", value });
      validation.label = `${cause} — ${remedy}`;
      get("BuildMode.Placement.Validation.Cause").label = cause;
      get("BuildMode.Placement.Validation.Remedy").label = remedy;
      get("BuildMode.Action.Confirm").state.enabled = !error;
      return true;
    };
    if (id === "BuildMode.Tool.Road") return setTool("road");
    if (id === "BuildMode.Tool.Warehouse") return setTool("warehouse");
    if (id === "BuildMode.Map.RoadTarget" && this.placement.tool === "road") {
      this.placement.preview = "road";
      const preview = get("BuildMode.Placement.Preview");
      preview.state = semanticState({ selected: true, valueType: "placement-preview", value: "definition=Building.Road;anchor=18,16;canPlace=true" });
      get("BuildMode.Placement.Validation").state = semanticState({ selected: true, valueType: "placement-validation", value: "None" });
      get("BuildMode.Action.Confirm").state.enabled = true;
      return true;
    }
    if (id === "BuildMode.Map.InvalidTarget" && this.placement.tool === "warehouse") {
      this.placement.preview = "invalid";
      return validate(true, "RoadRequired", "Road required", "Build next to a road");
    }
    if (id === "BuildMode.Map.ValidTarget" && this.placement.tool === "warehouse" && this.placement.placedBuildingCount >= 1) {
      this.placement.preview = "valid";
      return validate(false, "None", "Valid placement", "Confirm placement");
    }
    if (id === "BuildMode.Action.Confirm" && get(id).state.enabled) {
      this.placement.placedBuildingCount += 1;
      this.placement.tick += 1;
      this.fixture.tick = this.placement.tick;
      const buildingId = this.placement.nextBuildingId;
      this.placement.nextBuildingId += 1;
      const warehouse = this.placement.tool === "warehouse";
      const construction = {
        buildingId,
        cityId: "City.Lubeck",
        buildingDefinitionId: warehouse ? "Building.Warehouse" : "Building.Road",
        state: "UnderConstruction",
        startedTick: this.fixture.tick,
        elapsedTicks: 0,
        totalTicks: warehouse ? 30 : 1,
        progressPartsPerMillion: 0,
        paidCurrencyPfennig: warehouse ? 2500 : 25,
        cancellationCurrencyRefundPfennig: warehouse ? 1250 : 12,
      };
      this.placement.constructions.set(buildingId, construction);
      const result = get("BuildMode.Result.Building");
      result.state = semanticState({ selected: true, valueType: "building-entity-id", value: `Building:${buildingId}:0` });
      result.label = `Committed ${result.state.value}`;
      const status = get("BuildMode.Construction.Status");
      status.state = semanticState({ visible: true, valueType: "construction-state", value: `building=Building:${buildingId}:0;state=UnderConstruction;elapsedTicks=0;totalTicks=${construction.totalTicks};progressPpm=0` });
      status.label = `UnderConstruction: 0/${construction.totalTicks} ticks`;
      const cost = get("BuildMode.Construction.Cost");
      cost.state = semanticState({ visible: true, selected: true, valueType: "construction-cost", value: `definition=${construction.buildingDefinitionId};affordable=true;requiredCurrencyPfennig=${construction.paidCurrencyPfennig};missingCurrencyPfennig=0` });
      cost.label = `Affordable: ${construction.paidCurrencyPfennig} pfennig`;
      get(id).state.enabled = false;
      return true;
    }
    if (id === "BuildMode.Action.Cancel") return setTool("");
    if (id === "BuildMode.Action.Repeat") { get(id).state.selected = !get(id).state.selected; get(id).state.value = String(get(id).state.selected); return true; }
    if (id === "BuildMode.Action.Rotate" && this.placement.tool === "warehouse") return true;
    return false;
  }

  #matches(predicate) {
    if (!predicate || typeof predicate !== "object") return null;
    if (predicate.kind === "production.completed_cycles_at_least" && this.fixture.cycles.has(predicate.productionId)) return this.fixture.cycles.get(predicate.productionId) >= predicate.minimumCompletedCycles;
    if (predicate.kind === "production.blocker_equals" && this.fixture.cycles.has(predicate.productionId)) return predicate.blocker === "None";
    if (this.fixture.integrated) {
      if (predicate.kind === "integrated.construction_completed") return this.fixture.integrated.constructionCompleted;
      if (predicate.kind === "integrated.inventory_moved") return this.fixture.integrated.inventoryMoved;
      if (predicate.kind === "integrated.production_completed") return this.fixture.integrated.productionCompleted;
      if (predicate.kind === "integrated.population_grown") return this.fixture.integrated.populationGrown;
      if (predicate.kind === "integrated.bread_consumed") return this.fixture.integrated.breadConsumed;
    }
    if (!this.fixture.market || predicate.cityId !== "City.Lubeck" || predicate.goodId !== "Good.Grain") return null;
    if (predicate.kind === "market.alert_active") return this.fixture.market.alerts.includes(predicate.alertType);
    if (predicate.kind === "market.stock_at_least") return this.fixture.market.stock >= predicate.minimumStockMilliUnits;
    if (predicate.kind === "market.price_at_most") return this.fixture.market.price <= predicate.maximumPriceMilliMarks;
    if (predicate.kind === "market.reserve_recovered") return this.fixture.market.stock >= this.fixture.market.reserve && !this.fixture.market.alerts.includes("Shortage");
    return null;
  }

  #fixtureSummary(ticksAdvanced) {
    return { loaded: true, fixtureId: this.fixture.id, fixtureVersion: this.placement ? 1 : 2, registryHash: this.fixture.integrated ? "5330365030344C42" : (this.placement ? "534F35504C414345" : "B0481C9F740D6C18"), stateHash: `fake-${this.fixture.id}-${this.fixture.tick}`, tick: this.fixture.tick, eventCount: this.fixture.eventCount, productionCount: this.fixture.cycles.size, ticksAdvanced };
  }
}

/** Exercises the real codec in both directions without opening a pipe or starting Unreal. */
export class FakeInProcessTransport {
  constructor(endpoint = new FakeHansaEndpoint()) {
    this.endpoint = endpoint;
    this.closed = false;
  }

  async request(envelope) {
    if (this.closed) throw new Error("Fake transport is closed.");
    const serverDecoder = new FrameDecoder();
    const requestFrame = encodeFrame(envelope);
    const requests = [
      ...serverDecoder.push(requestFrame.subarray(0, 3)),
      ...serverDecoder.push(requestFrame.subarray(3)),
    ];
    const responseFrame = encodeFrame(this.endpoint.handle(requests[0]));
    if (responseFrame.length > MAX_FRAME_BYTES + 4) throw new Error("Fake response exceeded frame limit.");
    const clientDecoder = new FrameDecoder();
    return [
      ...clientDecoder.push(responseFrame.subarray(0, 7)),
      ...clientDecoder.push(responseFrame.subarray(7)),
    ][0];
  }

  close() {
    this.closed = true;
  }
}
