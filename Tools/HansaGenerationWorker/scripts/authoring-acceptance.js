import assert from "node:assert/strict";
import { readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { JobStore } from "../src/job-store.js";
import { GenerationWorkerService } from "../src/worker-service.js";
import { DeterministicMockProvider } from "../src/providers/mock-provider.js";

// Intentionally instantiate only the mock; ambient OpenAI settings cannot enable live calls.
const [contractPath, outputRoot] = process.argv.slice(2);
assert.ok(contractPath && outputRoot, "Usage: authoring-acceptance.js <native-contract.json> <evidence-root>");
const contract = JSON.parse(await readFile(contractPath, "utf8"));
const jobsRoot = path.join(outputRoot, "jobs");
const makeService = () => new GenerationWorkerService({ store: new JobStore(jobsRoot), providers: [new DeterministicMockProvider()], autoProcess: false });
let service = makeService();
try {
  await service.initialize();
  const request = {
    schemaVersion: 1, idempotencyKey: "s12-p04-recipe", providerId: "mock", modelVersion: "mock-v1",
    capability: "StructuredDataDraft", intendedAssetRole: "DefinitionPatch",
    prompt: "Propose one bounded recipe change to cycle time and labor; preserve identity and references.",
    parameters: { definitionProposal: contract }, inputArtifacts: [],
    budgets: { maximumCostMinorUnits: 0, currency: "USD", maximumOutputBytes: 1048576 },
    outputContract: { version: 1, maximumArtifacts: 1, allowedMediaTypes: ["application/json"] },
    timeoutMs: 30000,
    spendApproval: { approved: true, approvedBy: "S12-P04 mock acceptance", approvedAt: new Date().toISOString() },
  };
  const submitted = await service.submit(request);
  assert.equal(submitted.job.status, "Queued");
  await service.closeForRestart();
  service = makeService();
  await service.initialize();
  await service.schedule(submitted.job.jobId);
  const job = await service.get(submitted.job.jobId);
  assert.equal(job.status, "Review");
  assert.ok(job.resumeCount > 0);
  const output = await service.readOutput(job.jobId, 0); // Verifies stored size and SHA-256.
  assert.equal(output.document.patch.CycleTicks, contract.baseValues.CycleTicks + 1);
  assert.equal(output.document.patch.LaborerWorkforce, contract.baseValues.LaborerWorkforce + 1);
  const resumed = await service.resume(job.jobId);
  assert.equal(resumed.providerJobId, job.providerJobId);
  assert.equal((await service.list()).length, 1);
  await writeFile(path.join(outputRoot, "worker-proposal.json"), JSON.stringify(output.document, null, 2));
  await writeFile(path.join(outputRoot, "worker-acceptance.json"), JSON.stringify({
    status: "Succeeded", provider: "mock", liveProviderCalls: false, recoveredFromQueued: true,
    jobId: job.jobId, providerJobId: job.providerJobId, outputSha256: output.sha256,
    manifests: job.manifests, provenance: job.provenance,
  }, null, 2));
  console.log("Native contract completed through persistent mock worker and verified staged output.");
} finally { await service.closeForRestart(); }
