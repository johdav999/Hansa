import { WorkerPipeClient } from "../src/pipe-client.js";
const jobId = process.argv[2];
const outputIndex = Number(process.argv[3] ?? 0);
if (!jobId || !Number.isSafeInteger(outputIndex) || outputIndex < 0) {
  throw new Error("Usage: node Tools/HansaGenerationWorker/scripts/retain-media.js <Hansa-job-UUID> [output-index]");
}
const client = new WorkerPipeClient({
  pipeName: process.env.HANSA_GENERATION_WORKER_PIPE ?? "hansa-generation-worker-v1",
  authenticationToken: process.env.HANSA_GENERATION_WORKER_TOKEN ?? "",
  timeoutMs: 30000,
});
const result = await client.call("job.media.retain", { jobId, outputIndex });
console.log(JSON.stringify(result, null, 2));
