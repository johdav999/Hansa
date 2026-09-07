import { hashDocument } from "../canonical-json.js";
import { WorkerError } from "../errors.js";
import { buildDefinitionProposalResponseSchema, normalizeDefinitionProposalContract, validateDefinitionProposal } from "../proposal-contract.js";

function requireConfiguration(value, name) {
  if (typeof value !== "string" || value.length === 0) throw new WorkerError("OpenAIConfigurationMissing", `${name} is required to enable the OpenAI adapter.`);
  return value;
}

function extractOutputText(response) {
  if (response?.status !== "completed") throw new WorkerError("OpenAIResponseIncomplete", "The OpenAI response did not complete.", { retryable: true });
  const parts = (response.output ?? []).flatMap((item) => item?.type === "message" ? item.content ?? [] : []);
  const refusal = parts.find((item) => item?.type === "refusal");
  if (refusal) throw new WorkerError("OpenAIRefusal", "OpenAI refused the bounded definition proposal.", { remedy: "Revise the proposal prompt without weakening the schema or approval boundary.", retryable: false });
  const text = parts.filter((item) => item?.type === "output_text" && typeof item.text === "string").map((item) => item.text).join("");
  if (!text) throw new WorkerError("MalformedProviderOutput", "OpenAI returned no structured output text.", { retryable: true });
  try { return JSON.parse(text); }
  catch { throw new WorkerError("ProseInsteadOfData", "OpenAI output was not exactly one JSON document.", { remedy: "Keep strict Structured Outputs enabled and retry the bounded request.", retryable: true }); }
}

function minorUnits(tokens, microdollarsPerMillion) {
  const numerator = BigInt(tokens) * BigInt(microdollarsPerMillion);
  return Number((numerator + 9_999_999_999n) / 10_000_000_000n);
}

export class OpenAIResponsesProvider {
  constructor({ apiKey, modelVersion, inputMicrodollarsPerMillion, outputMicrodollarsPerMillion, rateCardId, fetchImpl = globalThis.fetch, endpoint = "https://api.openai.com/v1/responses" } = {}) {
    this.providerId = "openai";
    this.adapterVersion = "1.0.0";
    this.apiKey = requireConfiguration(apiKey, "OpenAI API key");
    this.modelVersion = requireConfiguration(modelVersion, "Pinned OpenAI model version");
    this.rateCardId = requireConfiguration(rateCardId, "OpenAI rate card ID");
    this.inputRate = Number(inputMicrodollarsPerMillion);
    this.outputRate = Number(outputMicrodollarsPerMillion);
    if (!Number.isSafeInteger(this.inputRate) || this.inputRate < 0 || !Number.isSafeInteger(this.outputRate) || this.outputRate < 0) throw new WorkerError("OpenAIConfigurationInvalid", "OpenAI token rates must be nonnegative integral microdollars per million tokens.");
    if (typeof fetchImpl !== "function") throw new WorkerError("OpenAIConfigurationInvalid", "A fetch implementation is required.");
    this.fetch = fetchImpl;
    this.endpoint = endpoint;
  }

  getCapabilities() {
    return { providerId: this.providerId, adapterVersion: this.adapterVersion, models: [{ modelVersion: this.modelVersion, pinned: true }], capabilities: [{ capability: "StructuredDataDraft", asynchronous: true, supportsCancellation: true, supportsSeed: false, inputMediaTypes: [], outputMediaTypes: ["application/json"] }] };
  }

  validateRequest(request) {
    if (request.providerId !== this.providerId || request.modelVersion !== this.modelVersion) throw new WorkerError("ProviderRequestRejected", "The OpenAI request must use the configured pinned model version.");
    if (request.inputArtifacts?.length) throw new WorkerError("ProviderRequestRejected", "Definition proposals accept schema-derived context only; remove attached files before estimating.");
    const contract = normalizeDefinitionProposalContract(request);
    if (!request.outputContract.allowedMediaTypes.includes("application/json") || request.outputContract.maximumArtifacts !== 1) throw new WorkerError("ProviderRequestRejected", "OpenAI definition proposals require exactly one application/json artifact.");
    return { valid: true, contract };
  }

  estimateCost(request) {
    const contract = this.validateRequest(request).contract;
    // Include schema, instructions and envelope; padding bounds message framing.
    const estimatedInputTokens = Buffer.byteLength(JSON.stringify(this.buildRequestBody(request, contract)), "utf8") + 1024;
    return { currency: "USD", estimatedMinorUnits: minorUnits(estimatedInputTokens, this.inputRate) + minorUnits(contract.maxOutputTokens, this.outputRate), usageUnit: "token-bound", estimatedUsage: estimatedInputTokens + contract.maxOutputTokens, rateCardId: this.rateCardId };
  }

  buildRequestBody(request, contract) {
    const body = {
      model: this.modelVersion,
      store: false,
      max_output_tokens: contract.maxOutputTokens,
      input: [
        { role: "developer", content: [{ type: "input_text", text: "Produce only the requested Hansa definition patch. Preserve every base identity field. Use null for writable fields that should remain unchanged. Never emit prose, code, paths, URLs, or unknown references." }] },
        { role: "user", content: [{ type: "input_text", text: request.prompt }, { type: "input_text", text: `Deterministic proposal contract: ${JSON.stringify(contract)}` }] },
      ],
      text: { format: { type: "json_schema", name: "hansa_definition_patch", strict: true, schema: buildDefinitionProposalResponseSchema(contract) } },
      metadata: { hansa_contract_hash: contract.contractHash },
    };
    return body;
  }

  async submit(request, context) {
    const contract = this.validateRequest(request).contract;
    const body = this.buildRequestBody(request, contract);
    const timeout = AbortSignal.timeout(Math.max(1, Math.min(request.timeoutMs ?? 30000, context.remainingTimeoutMs ?? 300000)));
    const signal = context.signal ? AbortSignal.any([context.signal, timeout]) : timeout;
    let response;
    let raw;
    try {
      response = await this.fetch(this.endpoint, { method: "POST", headers: { Authorization: `Bearer ${this.apiKey}`, "Content-Type": "application/json", "Idempotency-Key": context.idempotencyKeyHash }, body: JSON.stringify(body), signal, redirect: "error" });
      if (!response?.ok) throw new WorkerError("OpenAIRequestFailed", `OpenAI Responses API returned HTTP ${Number(response?.status ?? 0)}.`, { remedy: "Inspect provider access and the pinned model.", retryable: Number(response?.status ?? 0) >= 500 || Number(response?.status ?? 0) === 429 });
      const maximumBytes = Math.min(4 * 1024 * 1024, (request.budgets?.maximumOutputBytes ?? 1048576) + 262144);
      const reader = response.body?.getReader();
      if (!reader) throw new WorkerError("MalformedProviderOutput", "OpenAI returned no readable response body.");
      const chunks = [];
      let size = 0;
      try {
        for (;;) {
          const { done, value } = await reader.read();
          if (done) break;
          size += value.byteLength;
          if (size > maximumBytes) throw new WorkerError("ProviderResponseTooLarge", "OpenAI response exceeded its bounded byte limit.");
          chunks.push(Buffer.from(value));
        }
        try { raw = JSON.parse(Buffer.concat(chunks).toString("utf8")); }
        catch { throw new WorkerError("MalformedProviderOutput", "OpenAI returned invalid response JSON."); }
      } finally { await reader.cancel().catch(() => {}); reader.releaseLock(); }
    } catch (error) {
      if (signal.aborted) throw new WorkerError("OpenAIRequestAborted", "The OpenAI request was cancelled or exceeded its deadline.", { retryable: true });
      if (error instanceof WorkerError) throw error;
      throw new WorkerError("OpenAITransportFailed", "The OpenAI Responses API request failed.", { retryable: true });
    }
    if (typeof raw?.id !== "string" || !/^resp_[A-Za-z0-9_-]{1,200}$/.test(raw.id) || raw.model !== this.modelVersion)
      throw new WorkerError("MalformedProviderOutput", "OpenAI response identity or pinned model did not match.");
    const usage = raw.usage;
    if (!usage || !Number.isSafeInteger(usage.input_tokens) || usage.input_tokens < 0 ||
        !Number.isSafeInteger(usage.output_tokens) || usage.output_tokens < 0 || usage.output_tokens > contract.maxOutputTokens)
      throw new WorkerError("MalformedProviderOutput", "OpenAI returned invalid or missing bounded token usage.");
    const validated = validateDefinitionProposal(extractOutputText(raw), contract);
    return { providerJobId: raw.id, state: { status: "completed", proposal: validated.proposal, proposalHash: validated.proposalHash, contractHash: contract.contractHash, model: raw.model, usage: { inputTokens: usage.input_tokens, outputTokens: usage.output_tokens } } };
  }

  async poll(providerJobId, state) {
    if (state?.status === "cancelled") return { status: "cancelled", state, progress: 0 };
    if (state?.status !== "completed") throw new WorkerError("MalformedProviderOutput", "OpenAI resumable state is invalid.", { retryable: true });
    const actualMinorUnits = minorUnits(state.usage.inputTokens, this.inputRate) + minorUnits(state.usage.outputTokens, this.outputRate);
    return { status: "completed", state, progress: 100, result: { resultContractVersion: 1, artifacts: [{ logicalName: "definition-proposal", mediaType: "application/json", contentBase64: Buffer.from(JSON.stringify(state.proposal), "utf8").toString("base64") }], usage: { currency: "USD", actualMinorUnits, usageUnit: "tokens", actualUsage: state.usage.inputTokens + state.usage.outputTokens }, providerJobId, proposalHash: state.proposalHash, contractHash: state.contractHash, model: state.model } };
  }

  async cancel(_providerJobId, state) { return { ...state, status: "cancelled", proposal: undefined }; }
  async download(result) { return result.artifacts; }
  normalizeMetadata(result) { return { providerId: this.providerId, adapterVersion: this.adapterVersion, modelVersion: this.modelVersion, providerJobId: result.providerJobId, proposalHash: result.proposalHash, contractHash: result.contractHash, rateCardId: this.rateCardId, store: false }; }
}

export function createOpenAIProviderFromEnvironment(environment = process.env, options = {}) {
  const required = ["OPENAI_API_KEY", "HANSA_OPENAI_MODEL", "HANSA_OPENAI_RATE_CARD_ID", "HANSA_OPENAI_INPUT_MICRODOLLARS_PER_MILLION", "HANSA_OPENAI_OUTPUT_MICRODOLLARS_PER_MILLION"];
  if (!required.every((name) => typeof environment[name] === "string" && environment[name].length > 0)) return null;
  return new OpenAIResponsesProvider({ apiKey: environment.OPENAI_API_KEY, modelVersion: environment.HANSA_OPENAI_MODEL, rateCardId: environment.HANSA_OPENAI_RATE_CARD_ID, inputMicrodollarsPerMillion: Number(environment.HANSA_OPENAI_INPUT_MICRODOLLARS_PER_MILLION), outputMicrodollarsPerMillion: Number(environment.HANSA_OPENAI_OUTPUT_MICRODOLLARS_PER_MILLION), ...options });
}
