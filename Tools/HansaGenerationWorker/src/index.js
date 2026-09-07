import path from "node:path";
import { fileURLToPath } from "node:url";
import { JobStore } from "./job-store.js";
import { DeterministicMockProvider } from "./providers/mock-provider.js";
import { createOpenAIProviderFromEnvironment } from "./providers/openai-responses-provider.js";
import { createTripoProviderFromEnvironment } from "./providers/tripo-static-prop-provider.js";
import { createElevenLabsProviderFromEnvironment } from "./providers/elevenlabs-audio-provider.js";
import { createLogger } from "./redaction.js";
import { WorkerRequestHandler } from "./request-handler.js";
import { WorkerPipeServer } from "./pipe-server.js";
import { GenerationWorkerService } from "./worker-service.js";

function argument(name) {
  const prefix = `--${name}=`;
  return process.argv.find((item) => item.startsWith(prefix))?.slice(prefix.length);
}

const moduleDirectory = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(moduleDirectory, "../../..");
const jobsRoot = path.resolve(argument("jobs-root") ?? process.env.HANSA_GENERATION_JOBS_ROOT ?? path.join(projectRoot, "Saved", "GenerationJobs"));
const pipeName = argument("pipe") ?? process.env.HANSA_GENERATION_WORKER_PIPE ?? "hansa-generation-worker-v1";
const authenticationToken = process.env.HANSA_GENERATION_WORKER_TOKEN ?? "";
const logger = createLogger({ secrets: [authenticationToken, process.env.TRIPO_API_KEY, process.env.ELEVENLABS_API_KEY].filter(Boolean) });

const store = new JobStore(jobsRoot, { logger });
const providers = [new DeterministicMockProvider()];
const openAIProvider = createOpenAIProviderFromEnvironment();
if (openAIProvider) providers.push(openAIProvider);
const tripoProvider = createTripoProviderFromEnvironment();
if (tripoProvider) providers.push(tripoProvider);
const elevenLabsProvider = createElevenLabsProviderFromEnvironment();
if (elevenLabsProvider) providers.push(elevenLabsProvider);
const service = new GenerationWorkerService({ store, providers, logger, projectRoot });
await service.initialize();
const handler = new WorkerRequestHandler({ service, authenticationToken });
const server = new WorkerPipeServer({ pipeName, handler, logger });
await server.listen();

let closing = false;
async function close(signal) {
  if (closing) return;
  closing = true;
  logger.log("info", "worker.stopping", { signal });
  await server.close();
  await service.closeForRestart();
  process.exitCode = 0;
}
process.on("SIGINT", () => close("SIGINT"));
process.on("SIGTERM", () => close("SIGTERM"));
