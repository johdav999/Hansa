import assert from "node:assert/strict";
import { readFile, rm, writeFile } from "node:fs/promises";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import os from "node:os";
import path from "node:path";
import { mkdtemp } from "node:fs/promises";
import { test } from "node:test";
import { JobStore } from "../src/job-store.js";
import { WorkerPipeClient } from "../src/pipe-client.js";
import { WorkerPipeServer } from "../src/pipe-server.js";
import { DeterministicMockProvider } from "../src/providers/mock-provider.js";
import { createLogger } from "../src/redaction.js";
import { WorkerRequestHandler } from "../src/request-handler.js";
import { GenerationWorkerService } from "../src/worker-service.js";
import { RESULT_V1_FIELDS } from "../src/contracts.js";

const TOKEN = "test-generation-token-12345";
const quietLogger = createLogger({ stream: { write() {} }, secrets: [TOKEN] });

function request(id, parameters = {}, overrides = {}) {
  return {
    schemaVersion: 1,
    idempotencyKey: id,
    capability: "StructuredDataDraft",
    intendedAssetRole: "Definition.Proposal",
    providerId: "mock",
    modelVersion: "mock-v1",
    prompt: "Propose a deterministic grain recipe.",
    negativePrompt: "No executable content.",
    inputArtifacts: [],
    rights: { declaration: "Owned project data" },
    budgets: { maximumCostMinorUnits: 0, currency: "USD", maximumOutputBytes: 65_536 },
    outputContract: { version: 1, maximumArtifacts: 1, allowedMediaTypes: ["application/json"] },
    seed: 1201,
    parameters,
    timeoutMs: 2_000,
    spendApproval: { approved: true, approvedBy: "Automation", approvedAt: "2026-09-06T00:00:00.000Z" },
    ...overrides,
  };
}

function definitionProposalContract() {
  return {
    contractVersion: 1,
    schemaId: "Hansa.RecipeDefinition",
    schemaVersion: 1,
    sourceSchemaHash: "a".repeat(64),
    baseStableId: "Recipe.MillFlour",
    baseRevision: 3,
    baseContentHash: "0123456789abcdef",
    writableFields: {
      CycleTicks: { type: "integer", minimum: 1, maximum: 1000, "x-hansa-reference": "None" },
      LaborerWorkforce: { type: "integer", minimum: 0, maximum: 100, "x-hansa-reference": "None" },
    },
    baseValues: { CycleTicks: 2, LaborerWorkforce: 4 },
    stableReferences: ["Recipe.MillFlour"],
    maxOutputTokens: 512,
  };
}
async function harness(options = {}) {
  const root = await mkdtemp(path.join(os.tmpdir(), "hansa-worker-"));
  const store = new JobStore(root, { logger: quietLogger });
  const service = new GenerationWorkerService({
    store,
    providers: [new DeterministicMockProvider()],
    logger: quietLogger,
    pollIntervalMs: options.pollIntervalMs ?? 5,
    autoProcess: options.autoProcess ?? true,
  });
  await service.initialize();
  return { root, store, service, async dispose() { await service.closeForRestart(); await rm(root, { recursive: true, force: true }); } };
}

async function waitFor(service, jobId, predicate, timeoutMs = 3_000) {
  const deadline = Date.now() + timeoutMs;
  let job;
  while (Date.now() < deadline) {
    job = await service.get(jobId);
    if (predicate(job)) return job;
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
  assert.fail(`Timed out waiting for job. Latest: ${JSON.stringify(job)}`);
}

test("read-only estimate precedes approval and creates no durable job", async () => {
  const h = await harness();
  try {
    const unapproved = request("estimate-job-0001", {}, { spendApproval: { approved: false } });
    const result = h.service.estimate(unapproved);
    assert.deepEqual(result.estimate, { currency: "USD", estimatedMinorUnits: 0, usageUnit: "mock-operation", estimatedUsage: 1 });
    assert.match(result.requestHash, /^[0-9a-f]{64}$/);
    assert.match(result.inputHash, /^[0-9a-f]{64}$/);
    assert.equal((await h.service.list()).length, 0);
    await assert.rejects(h.service.submit(unapproved), (error) => error.code === "SpendApprovalRequired");
  } finally { await h.dispose(); }
});
test("definition proposal mock produces a schema-valid two-field acceptance artifact", async () => {
  const h = await harness();
  try {
    const submitted = await h.service.submit(request("definition-proposal-mock-0001", { definitionProposal: definitionProposalContract() }, {
      intendedAssetRole: "DefinitionPatch",
      prompt: "Suggest bounded cycle and workforce values for Recipe.MillFlour.",
    }));
    const job = await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Review" && value.manifests.length === 1);
    const output = await h.service.readOutput(job.jobId, 0);
    assert.equal(output.document.baseStableId, "Recipe.MillFlour");
    assert.equal(output.document.patch.CycleTicks, 3);
    assert.equal(output.document.patch.LaborerWorkforce, 5);
    assert.match(job.provenance.contractHash, /^[0-9a-f]{64}$/);
    assert.match(job.provenance.proposalHash, /^[0-9a-f]{64}$/);
  } finally { await h.dispose(); }
});
test("successful mock job persists hashes, immutable manifest, redaction and idempotent submit", async () => {
  const h = await harness();
  try {
    const privateBytes = Buffer.from("private-reference-bytes", "utf8");
    const raw = request("success-job-0001", { proposal: { stableId: "Good.MockGrain", value: 7 } }, {
      prompt: "Use https://provider.example/signed?secret=abc for context.",
      inputArtifacts: [{ role: "Reference", mediaType: "text/plain", contentBase64: privateBytes.toString("base64"), rightsDeclaration: "Created for this test" }],
    });
    const first = await h.service.submit(raw);
    const duplicate = await h.service.submit(raw);
    assert.equal(first.created, true);
    assert.equal(duplicate.created, false);
    assert.equal(duplicate.job.jobId, first.job.jobId);
    await assert.rejects(h.service.submit({ ...raw, prompt: "A different request under the same key." }), (error) => error.code === "IdempotencyConflict");

    const job = await waitFor(h.service, first.job.jobId, (value) => value.status === "Review" && value.manifests.length === 1);
    assert.equal(job.outputs.length, 1);
    const readableOutput = await h.service.readOutput(job.jobId, 0);
    assert.equal(readableOutput.sha256, job.outputs[0].sha256);
    assert.equal(readableOutput.document.capability, "StructuredDataDraft");
    assert.match(job.requestHash, /^[0-9a-f]{64}$/);
    assert.match(job.inputHash, /^[0-9a-f]{64}$/);
    assert.match(job.outputs[0].sha256, /^[0-9a-f]{64}$/);
    assert.equal(job.qaResults.every((item) => item.passed), true);
    assert.equal(job.request.inputArtifacts[0].privateRelativePath, undefined);

    const manifestPath = path.join(h.root, job.jobId, job.manifests[0].relativePath);
    const manifestText = await readFile(manifestPath, "utf8");
    assert.doesNotMatch(manifestText, /provider\.example|private-reference-bytes|signed\?secret/);
    assert.match(manifestText, /REDACTED_URL/);
    assert.match(manifestText, /manifestHash/);

    const changed = await h.store.getJob(job.jobId);
    changed.progress.message = "Conflicting rewrite";
    await assert.rejects(h.store.writeManifest(changed, { providerId: "mock" }), (error) => error.code === "ImmutableRecordConflict");
  } finally {
    await h.dispose();
  }
});

test("malformed provider output fails with a structured retryable error", async () => {
  const h = await harness();
  try {
    const submitted = await h.service.submit(request("malformed-job-0001", { mockMode: "malformed" }));
    const job = await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Failed" && value.manifests.length === 1);
    assert.equal(job.errors[0].code, "MalformedProviderOutput");
    assert.equal(job.errors[0].retryable, true);
    assert.equal(job.outputs.length, 0);
    assert.doesNotMatch(await readFile(path.join(h.root, job.jobId, "job.json"), "utf8"), /must-not-persist/);
  } finally { await h.dispose(); }
});

test("timeout expires deterministically and records a manifest", async () => {
  const h = await harness({ pollIntervalMs: 5 });
  try {
    const submitted = await h.service.submit(request("timeout-job-0001", { mockMode: "timeout" }, { timeoutMs: 30 }));
    const job = await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Expired" && value.manifests.length === 1);
    assert.equal(job.errors[0].code, "ProviderTimeout");
    assert.equal(job.errors[0].retryable, true);
  } finally { await h.dispose(); }
});

test("cancellation reaches provider and persists Cancelled", async () => {
  const h = await harness({ pollIntervalMs: 5 });
  try {
    const submitted = await h.service.submit(request("cancel-job-0001", { completeAfterPolls: 1000 }, { timeoutMs: 2_000 }));
    await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Running" && Boolean(value.providerJobId));
    await h.service.cancel(submitted.job.jobId);
    const job = await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Cancelled" && value.manifests.length === 1);
    assert.equal(job.errors[0].code, "CancelledByUser");
    assert.equal(job.cancellationRequested, true);
  } finally { await h.dispose(); }
});

test("retry preserves lineage and succeeds once without duplicate retry", async () => {
  const h = await harness();
  try {
    const submitted = await h.service.submit(request("retry-job-0001", { mockMode: "transientFailure", failThroughRevision: 1 }));
    const failed = await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Failed" && value.manifests.length === 1);
    assert.equal(failed.errors[0].code, "MockTransientFailure");
    const retried = await h.service.retry(failed.jobId, "retry-operation-0001", "Provider recovered", { approved: true, approvedBy: "Automation Retry", approvedAt: "2026-09-06T00:01:00.000Z" });
    assert.equal(retried.retried, true);
    const completed = await waitFor(h.service, failed.jobId, (value) => value.status === "Review" && value.manifests.length === 2);
    assert.equal(completed.revision, 2);
    assert.equal(completed.attempt, 2);
    assert.equal(completed.retryLineage.length, 1);
    const duplicateRetry = await h.service.retry(failed.jobId, "retry-operation-0001", "Duplicate command", { approved: true, approvedBy: "Automation Retry", approvedAt: "2026-09-06T00:01:00.000Z" });
    assert.equal(duplicateRetry.retried, false);
    assert.equal(duplicateRetry.job.revision, 2);
  } finally { await h.dispose(); }
});

test("worker restart resumes the same provider task and resume is idempotent", async () => {
  const h = await harness({ pollIntervalMs: 15 });
  let secondService;
  try {
    const submitted = await h.service.submit(request("restart-job-0001", { completeAfterPolls: 30 }, { timeoutMs: 3_000 }));
    const running = await waitFor(h.service, submitted.job.jobId, (value) => value.status === "Running" && Boolean(value.providerJobId));
    const providerJobId = running.providerJobId;
    await h.service.closeForRestart();

    const secondStore = new JobStore(h.root, { logger: quietLogger });
    secondService = new GenerationWorkerService({ store: secondStore, providers: [new DeterministicMockProvider()], logger: quietLogger, pollIntervalMs: 5 });
    await secondService.initialize();
    await Promise.all([secondService.resume(running.jobId), secondService.resume(running.jobId)]);
    const completed = await waitFor(secondService, running.jobId, (value) => value.status === "Review" && value.manifests.length === 1);
    assert.equal(completed.providerJobId, providerJobId);
    assert.equal(completed.resumeCount, 1);
    assert.equal(completed.outputs.length, 1);
  } finally {
    if (secondService) await secondService.closeForRestart();
    await rm(h.root, { recursive: true, force: true });
  }
});

test("startup repairs an interrupted idempotency-index update without duplicating the job", async () => {
  const h = await harness({ autoProcess: false });
  let restartedService;
  try {
    const raw = request("index-recovery-job-0001");
    const submitted = await h.service.submit(raw);
    await h.service.closeForRestart();
    await writeFile(path.join(h.root, "idempotency-index.json"), JSON.stringify({ schemaVersion: 1, submissions: {} }), "utf8");

    const restartedStore = new JobStore(h.root, { logger: quietLogger });
    restartedService = new GenerationWorkerService({ store: restartedStore, providers: [new DeterministicMockProvider()], logger: quietLogger, autoProcess: false });
    await restartedService.initialize();
    const duplicate = await restartedService.submit(raw);
    assert.equal(duplicate.created, false);
    assert.equal(duplicate.job.jobId, submitted.job.jobId);
    assert.equal((await restartedService.list()).length, 1);
  } finally {
    if (restartedService) await restartedService.closeForRestart();
    await rm(h.root, { recursive: true, force: true });
  }
});

test("published result schema and runtime result fields stay in lockstep", async () => {
  const schemaPath = fileURLToPath(new URL("../schemas/generation-result-v1.schema.json", import.meta.url));
  const schema = JSON.parse(await readFile(schemaPath, "utf8"));
  assert.equal(schema.additionalProperties, false);
  assert.deepEqual(Object.keys(schema.properties).sort(), [...RESULT_V1_FIELDS].sort());

  const provider = new DeterministicMockProvider();
  const raw = request("result-contract-job-0001", { definitionProposal: definitionProposalContract() }, { intendedAssetRole: "DefinitionPatch" });
  const submitted = await provider.submit(raw, { idempotencyKeyHash: "c".repeat(64), revision: 1 });
  const completed = await provider.poll(submitted.providerJobId, submitted.state, { request: raw, revision: 1, attempt: 1 });
  assert.equal(completed.status, "completed");
  assert.equal(Object.keys(completed.result).every((field) => RESULT_V1_FIELDS.includes(field)), true);
  assert.match(completed.result.contractHash, new RegExp(schema.properties.contractHash.pattern));
  assert.match(completed.result.proposalHash, new RegExp(schema.properties.proposalHash.pattern));
});
test("authenticated versioned named-pipe protocol exposes provider-neutral capabilities", async () => {
  const h = await harness();
  const pipeName = `hansa-worker-test-${process.pid}-${Date.now()}`;
  const handler = new WorkerRequestHandler({ service: h.service, authenticationToken: TOKEN });
  const server = new WorkerPipeServer({ pipeName, handler, logger: quietLogger });
  try {
    await server.listen();
    const client = new WorkerPipeClient({ pipeName, authenticationToken: TOKEN });
    const capabilities = await client.call("worker.capabilities");
    assert.equal(capabilities.protocolVersion, "1.0");
    assert.equal(capabilities.providers[0].providerId, "mock");
    assert.equal(capabilities.providers[0].capabilities[0].capability, "StructuredDataDraft");
    const estimated = await client.call("job.estimate", { request: request("pipe-estimate-0001", {}, { spendApproval: { approved: false } }) });
    assert.equal(estimated.estimate.estimatedMinorUnits, 0);
    assert.equal((await h.service.list()).length, 0);
    const rejected = new WorkerPipeClient({ pipeName, authenticationToken: "incorrect-token-1234" });
    await assert.rejects(rejected.call("worker.capabilities"), (error) => error.code === "AuthenticationFailed");
  } finally {
    await server.close();
    await h.dispose();
  }
});

test("credentials are rejected rather than persisted", async () => {
  const h = await harness();
  try {
    await assert.rejects(h.service.submit(request("secret-job-0001", {}, { parameters: { apiKey: "must-not-persist" } })), (error) => error.code === "ForbiddenSensitiveField");
    await assert.rejects(h.service.submit(request("secret-job-0002", {}, { parameters: { sessionToken: "must-not-persist" } })), (error) => error.code === "ForbiddenSensitiveField");
    assert.equal((await h.service.list()).length, 0);
  } finally { await h.dispose(); }
});

async function stopChild(child) {
  if (!child || child.exitCode !== null) return;
  const exited = new Promise((resolve) => child.once("exit", resolve));
  child.kill();
  await Promise.race([exited, new Promise((resolve) => setTimeout(resolve, 2_000))]);
  if (child.exitCode === null) child.kill("SIGKILL");
}

async function callEventually(client, operation, payload = {}, timeoutMs = 4_000) {
  const deadline = Date.now() + timeoutMs;
  let lastError;
  while (Date.now() < deadline) {
    try { return await client.call(operation, payload); }
    catch (error) { lastError = error; await new Promise((resolve) => setTimeout(resolve, 25)); }
  }
  throw lastError;
}

test("worker executable survives a real process crash and resumes through the same protocol", async () => {
  const root = await mkdtemp(path.join(os.tmpdir(), "hansa-worker-process-"));
  const pipeName = `hansa-worker-process-${process.pid}-${Date.now()}`;
  const entry = fileURLToPath(new URL("../src/index.js", import.meta.url));
  const start = () => spawn(process.execPath, [entry, `--pipe=${pipeName}`, `--jobs-root=${root}`], {
    env: { ...process.env, HANSA_GENERATION_WORKER_TOKEN: TOKEN },
    stdio: ["ignore", "ignore", "pipe"],
    windowsHide: true,
  });
  const client = new WorkerPipeClient({ pipeName, authenticationToken: TOKEN, timeoutMs: 500 });
  let first;
  let second;
  try {
    first = start();
    await callEventually(client, "worker.capabilities");
    const submitted = await client.call("job.submit", { request: request("process-restart-job-0001", { completeAfterPolls: 100 }, { timeoutMs: 8_000 }) });
    const jobId = submitted.job.jobId;
    let running;
    const runningDeadline = Date.now() + 3_000;
    do {
      running = (await client.call("job.get", { jobId })).job;
      if (running.status === "Running" && running.providerJobId) break;
      await new Promise((resolve) => setTimeout(resolve, 20));
    } while (Date.now() < runningDeadline);
    assert.equal(running.status, "Running");
    const providerJobId = running.providerJobId;
    await stopChild(first);

    second = start();
    await callEventually(client, "worker.capabilities");
    let completed;
    const completedDeadline = Date.now() + 8_000;
    do {
      completed = (await client.call("job.get", { jobId })).job;
      if (completed.status === "Review" && completed.manifests.length === 1) break;
      await new Promise((resolve) => setTimeout(resolve, 25));
    } while (Date.now() < completedDeadline);
    assert.equal(completed.status, "Review");
    assert.equal(completed.providerJobId, providerJobId);
    assert.equal(completed.resumeCount, 1);
    assert.equal(completed.manifests.length, 1);
  } finally {
    await stopChild(first);
    await stopChild(second);
    await rm(root, { recursive: true, force: true });
  }
});



test("cancelling an in-flight submission aborts its signal and writes Cancelled evidence", async () => {
  const h = await harness();
  try {
    let started;
    const ready = new Promise(resolve => { started = resolve; });
    let observedSignal;
    const adapter = h.service.providers.get("mock");
    adapter.submit = async (_request, context) => {
      observedSignal = context.signal;
      started();
      return new Promise((_resolve, reject) => context.signal.addEventListener("abort", () => reject(context.signal.reason), { once: true }));
    };
    const { job } = await h.service.submit(request("cancel-submitting"));
    await ready;
    await h.service.cancel(job.jobId);
    const cancelled = await waitFor(h.service, job.jobId, value => value.status === "Cancelled" && value.manifests.length > 0);
    assert.equal(observedSignal.aborted, true);
    assert.ok(cancelled.manifests.length > 0);
    assert.equal(cancelled.outputs.length, 0);
  } finally { await h.dispose(); }
});
