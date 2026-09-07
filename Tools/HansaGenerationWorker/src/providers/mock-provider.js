import { hashDocument } from "../canonical-json.js";
import { WorkerError } from "../errors.js";
import { normalizeDefinitionProposalContract, validateDefinitionProposal } from "../proposal-contract.js";

export function assertProviderContract(provider) {
  const methods = ["getCapabilities", "validateRequest", "estimateCost", "submit", "poll", "cancel", "download", "normalizeMetadata"];
  for (const method of methods) {
    if (typeof provider?.[method] !== "function") {
      throw new WorkerError("InvalidProviderAdapter", `Provider adapter is missing ${method}().`);
    }
  }
}

export class DeterministicMockProvider {
  constructor() {
    this.providerId = "mock";
    this.adapterVersion = "1.0.0";
  }

  getCapabilities() {
    return {
      providerId: this.providerId,
      adapterVersion: this.adapterVersion,
      models: [{ modelVersion: "mock-v1", pinned: true }],
      capabilities: [{
        capability: "StructuredDataDraft",
        asynchronous: true,
        supportsCancellation: true,
        supportsSeed: true,
        inputMediaTypes: ["application/json", "text/plain"],
        outputMediaTypes: ["application/json"],
      }],
    };
  }

  validateRequest(request) {
    if (request.providerId !== this.providerId || request.modelVersion !== "mock-v1") {
      throw new WorkerError("ProviderRequestRejected", "The deterministic mock accepts providerId=mock and modelVersion=mock-v1.");
    }
    if (request.capability !== "StructuredDataDraft") {
      throw new WorkerError("ProviderRequestRejected", "The deterministic mock supports StructuredDataDraft only.");
    }
    if (request.intendedAssetRole === "DefinitionPatch") return { valid: true, contract: normalizeDefinitionProposalContract(request) };
    return { valid: true };
  }

  definitionProposal(contract) {
    const patch = Object.fromEntries(Object.keys(contract.writableFields).map((name) => [name, null]));
    const preferred = ["CycleTicks", "LaborerWorkforce", "CostResearchPoints", "DurationTicks", "BaseValueMilliMarks"];
    let changed = 0;
    for (const name of preferred) {
      const schema = contract.writableFields[name];
      const current = contract.baseValues[name];
      if (!schema || (schema.type !== "integer" && schema.type !== "number") || typeof current !== "number") continue;
      const up = current + 1;
      const down = current - 1;
      const candidate = schema.maximum === undefined || up <= schema.maximum ? up : down;
      if ((schema.minimum === undefined || candidate >= schema.minimum) && Number.isSafeInteger(candidate)) {
        patch[name] = candidate;
        if (++changed === 2) break;
      }
    }
    if (changed === 0) throw new WorkerError("MockProposalUnavailable", "The deterministic mock needs at least one bounded numeric writable field for a DefinitionPatch demo.");
    return {
      proposalVersion: 1,
      schemaId: contract.schemaId,
      schemaVersion: contract.schemaVersion,
      sourceSchemaHash: contract.sourceSchemaHash,
      baseStableId: contract.baseStableId,
      baseRevision: contract.baseRevision,
      baseContentHash: contract.baseContentHash,
      patch,
    };
  }
  estimateCost() {
    return { currency: "USD", estimatedMinorUnits: 0, usageUnit: "mock-operation", estimatedUsage: 1 };
  }

  async submit(request, context) {
    return {
      providerJobId: `mock-${hashDocument({ key: context.idempotencyKeyHash, revision: context.revision }).slice(0, 24)}`,
      state: { polls: 0, cancelled: false },
    };
  }

  async poll(providerJobId, state, context) {
    const nextState = { ...state, polls: (state?.polls ?? 0) + 1 };
    if (nextState.cancelled) return { status: "cancelled", state: nextState, progress: 0 };
    const mode = context.request.parameters.mockMode ?? "success";
    if (mode === "timeout") return { status: "pending", state: nextState, progress: 25 };
    if (mode === "transientFailure" && context.revision <= Number(context.request.parameters.failThroughRevision ?? 1)) {
      return {
        status: "failed",
        state: nextState,
        error: { code: "MockTransientFailure", message: "The deterministic mock injected a retryable failure.", remedy: "Retry the job.", retryable: true },
      };
    }
    const completeAfterPolls = Number(context.request.parameters.completeAfterPolls ?? 1);
    if (nextState.polls < completeAfterPolls) {
      return { status: "pending", state: nextState, progress: Math.min(95, nextState.polls * 20) };
    }
    if (mode === "malformed") {
      return {
        status: "completed",
        state: nextState,
        progress: 100,
        result: {
          resultContractVersion: 1,
          artifacts: [{
            logicalName: "malformed",
            mediaType: "application/json",
            contentBase64: Buffer.from("{}", "utf8").toString("base64"),
            unexpectedPrivateValue: "must-not-persist",
          }],
          usage: { currency: "USD", actualMinorUnits: 0, usageUnit: "mock-operation", actualUsage: 1 },
          providerJobId,
        },
      };
    }
    let payload = {
      schemaVersion: 1,
      capability: context.request.capability,
      intendedAssetRole: context.request.intendedAssetRole,
      promptSha256: context.request.promptSha256,
      seed: context.request.seed,
      proposal: context.request.parameters.proposal ?? { stableId: "Mock.Generated.V1", value: 42 },
    };
    let proposalMetadata = {};
    if (context.request.intendedAssetRole === "DefinitionPatch") {
      const contract = normalizeDefinitionProposalContract(context.request);
      const validated = validateDefinitionProposal(this.definitionProposal(contract), contract);
      payload = validated.proposal;
      proposalMetadata = { contractHash: validated.contractHash, proposalHash: validated.proposalHash };
    }
    return {
      status: "completed",
      state: nextState,
      progress: 100,
      result: {
        resultContractVersion: 1,
        artifacts: [{
          logicalName: "structured-proposal",
          mediaType: "application/json",
          contentBase64: Buffer.from(JSON.stringify(payload), "utf8").toString("base64"),
        }],
        usage: { currency: "USD", actualMinorUnits: 0, usageUnit: "mock-operation", actualUsage: 1 },
        providerJobId,
        ...proposalMetadata,
      },
    };
  }

  async cancel(_providerJobId, state) {
    return { ...state, cancelled: true };
  }

  async download(result) {
    return result.artifacts;
  }

  normalizeMetadata(result) {
    return {
      providerId: this.providerId,
      adapterVersion: this.adapterVersion,
      modelVersion: "mock-v1",
      providerJobId: result.providerJobId,
      deterministicMock: true,
      contractHash: result.contractHash ?? null,
      proposalHash: result.proposalHash ?? null,
    };
  }
}
