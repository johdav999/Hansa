import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import path from "node:path";
import { test } from "node:test";
import { OpenAIResponsesProvider, createOpenAIProviderFromEnvironment } from "../src/providers/openai-responses-provider.js";

const here = path.dirname(fileURLToPath(import.meta.url));
const recorded = JSON.parse(await readFile(path.join(here, "recordings", "openai-definition-proposal-completed.json"), "utf8"));

function contract() {
  return {
    contractVersion: 1,
    schemaId: "Hansa.GoodDefinition",
    schemaVersion: 1,
    sourceSchemaHash: "a".repeat(64),
    baseStableId: "Good.Grain",
    baseRevision: 7,
    baseContentHash: "0123456789abcdef",
    writableFields: {
      BaseValueMilliMarks: { type: "integer", minimum: 1, maximum: 1000000000, "x-hansa-reference": "None" },
      DefinitionCategory: { type: "string", "x-hansa-reference": "None" },
      RecipeIds: { type: "array", items: { type: "string", "x-hansa-reference": "Recipe" }, "x-hansa-reference": "Recipe" }
    },
    baseValues: {
      BaseValueMilliMarks: 1000,
      DefinitionCategory: "Goods",
      RecipeIds: ["Recipe.Bread"]
    },
    stableReferences: ["Good.Grain", "Recipe.Bread"],
    maxOutputTokens: 512
  };
}

function request() {
  return {
    providerId: "openai", modelVersion: "gpt-6-astra", capability: "StructuredDataDraft", intendedAssetRole: "DefinitionPatch",
    prompt: "Increase the base value while preserving stable identity.", parameters: { definitionProposal: contract() },
    outputContract: { maximumArtifacts: 1, allowedMediaTypes: ["application/json"] }
  };
}

function provider(response = recorded, capture = {}) {
  return new OpenAIResponsesProvider({
    apiKey: "recorded-test-key-never-logged", modelVersion: "gpt-6-astra", rateCardId: "test-2026-09-06",
    inputMicrodollarsPerMillion: 1250000, outputMicrodollarsPerMillion: 10000000,
    fetchImpl: async (url, options) => { capture.url = url; capture.options = options; return new Response(JSON.stringify(response), { status: 200 }); }
  });
}

function withOutput(value) {
  const response = structuredClone(recorded);
  response.output[0].content[0].text = typeof value === "string" ? value : JSON.stringify(value);
  return response;
}

test("OpenAI adapter sends a pinned non-retained Responses request with strict JSON Schema", async () => {
  const capture = {};
  const adapter = provider(recorded, capture);
  const estimate = adapter.estimateCost(request());
  assert.equal(estimate.currency, "USD");
  assert.ok(estimate.estimatedMinorUnits > 0);
  const submitted = await adapter.submit(request(), { idempotencyKeyHash: "b".repeat(64), revision: 1 });
  assert.equal(submitted.providerJobId, recorded.id);
  const body = JSON.parse(capture.options.body);
  assert.equal(capture.url, "https://api.openai.com/v1/responses");
  assert.equal(capture.options.headers.Authorization, "Bearer recorded-test-key-never-logged");
  assert.equal(JSON.stringify(body).includes("recorded-test-key"), false);
  assert.equal(body.model, "gpt-6-astra");
  assert.equal(body.store, false);
  assert.equal(body.text.format.type, "json_schema");
  assert.equal(body.text.format.strict, true);
  assert.equal(body.text.format.schema.additionalProperties, false);
  assert.equal(JSON.stringify(body.text.format.schema).includes("x-hansa-reference"), false);
  assert.deepEqual(body.text.format.schema.properties.patch.required.sort(), ["BaseValueMilliMarks", "DefinitionCategory", "RecipeIds"].sort());
  const polled = await adapter.poll(submitted.providerJobId, submitted.state);
  assert.equal(polled.status, "completed");
  assert.equal(polled.result.artifacts[0].mediaType, "application/json");
  assert.equal(JSON.parse(Buffer.from(polled.result.artifacts[0].contentBase64, "base64")).patch.BaseValueMilliMarks, 1250);
  assert.equal(adapter.normalizeMetadata(polled.result).store, false);
});

test("OpenAI adapter rejects prose, stale bases, unknown fields, references and schema mismatches", async () => {
  await assert.rejects(provider(withOutput("Here is your proposal.")).submit(request(), { idempotencyKeyHash: "c".repeat(64) }), (error) => error.code === "ProseInsteadOfData");
  const base = JSON.parse(recorded.output[0].content[0].text);
  await assert.rejects(provider(withOutput({ ...base, baseRevision: 6 })).submit(request(), { idempotencyKeyHash: "d".repeat(64) }), (error) => error.code === "SchemaMismatch");
  await assert.rejects(provider(withOutput({ ...base, sourceSchemaHash: "b".repeat(64) })).submit(request(), { idempotencyKeyHash: "3".repeat(64) }), (error) => error.code === "SchemaMismatch");
  await assert.rejects(provider(withOutput({ ...base, surprise: true })).submit(request(), { idempotencyKeyHash: "e".repeat(64) }), (error) => error.code === "UnknownProposalField");
  await assert.rejects(provider(withOutput({ ...base, patch: { ...base.patch, RecipeIds: ["Recipe.Unknown"] } })).submit(request(), { idempotencyKeyHash: "f".repeat(64) }), (error) => error.code === "UnknownStableReference");
  await assert.rejects(provider(withOutput({ ...base, patch: { ...base.patch, BaseValueMilliMarks: "expensive" } })).submit(request(), { idempotencyKeyHash: "1".repeat(64) }), (error) => error.code === "SchemaMismatch");
  await assert.rejects(provider(withOutput({ ...base, patch: { ...base.patch, BaseValueMilliMarks: Number.MAX_SAFE_INTEGER + 1 } })).submit(request(), { idempotencyKeyHash: "2".repeat(64) }), (error) => error.code === "SchemaMismatch");
});

test("OpenAI adapter is absent unless every credential, pinned-model and rate-card input is injected", () => {
  assert.equal(createOpenAIProviderFromEnvironment({}), null);
  const configured = createOpenAIProviderFromEnvironment({ OPENAI_API_KEY: "test", HANSA_OPENAI_MODEL: "gpt-6-astra", HANSA_OPENAI_RATE_CARD_ID: "recorded", HANSA_OPENAI_INPUT_MICRODOLLARS_PER_MILLION: "1", HANSA_OPENAI_OUTPUT_MICRODOLLARS_PER_MILLION: "1" }, { fetchImpl: async () => undefined });
  assert.equal(configured.getCapabilities().models[0].modelVersion, "gpt-6-astra");
});


test("OpenAI estimates cover the full request and refuse unhandled attachments", async () => {
  const capture = {};
  const adapter = provider(recorded, capture);
  const estimate = adapter.estimateCost(request());
  await adapter.submit(request(), { idempotencyKeyHash: "a".repeat(64) });
  assert.ok(estimate.estimatedUsage >= Buffer.byteLength(capture.options.body) + contract().maxOutputTokens);
  assert.equal(capture.options.redirect, "error");
  assert.ok(capture.options.signal instanceof AbortSignal);
  assert.throws(() => adapter.estimateCost({ ...request(), inputArtifacts: [{ mediaType: "text/plain" }] }), { code: "ProviderRequestRejected" });
});

test("OpenAI rejects malformed usage, model identity, refusal, and inherited property names", async () => {
  for (const mutate of [r => { r.usage.input_tokens = -1; }, r => { r.usage.output_tokens = 1.5; }, r => { delete r.usage; }, r => { r.model = "unexpected-model"; }, r => { delete r.id; }]) {
    const response = structuredClone(recorded); mutate(response);
    await assert.rejects(provider(response).submit(request(), {}), { code: "MalformedProviderOutput" });
  }
  const refused = structuredClone(recorded);
  refused.output[0].content = [{ type: "refusal", refusal: "Mock refusal" }];
  await assert.rejects(provider(refused).submit(request(), {}), { code: "OpenAIRefusal" });
  const proposal = JSON.parse(recorded.output[0].content[0].text);
  proposal.patch.constructor = "forbidden";
  await assert.rejects(provider(withOutput(proposal)).submit(request(), {}), { code: "UnknownProposalField" });
});

test("OpenAI enforces response byte bounds and passes cancellation to fetch", async () => {
  const adapter = provider();
  adapter.fetch = async () => new Response("x".repeat(300000));
  await assert.rejects(adapter.submit({ ...request(), budgets: { maximumOutputBytes: 1 } }, {}), { code: "ProviderResponseTooLarge" });
  const cancellation = new AbortController();
  adapter.fetch = async (_url, options) => { cancellation.abort(); options.signal.throwIfAborted(); };
  await assert.rejects(adapter.submit(request(), { signal: cancellation.signal }), { code: "OpenAIRequestAborted" });
  adapter.fetch = async (_url, options) => new Promise((_resolve, reject) => {
    const keepAlive = setTimeout(() => reject(new Error("test deadline")), 1000);
    options.signal.addEventListener("abort", () => { clearTimeout(keepAlive); reject(options.signal.reason); }, { once: true });
  });
  await assert.rejects(adapter.submit({ ...request(), timeoutMs: 25 }, {}), { code: "OpenAIRequestAborted" });
});
