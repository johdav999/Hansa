import { PROTOCOL_NAME, PROTOCOL_VERSION } from "./contracts.js";
import { structuredError, WorkerError } from "./errors.js";
import { redactValue } from "./redaction.js";
import { validateEnvelope } from "./protocol.js";

export class WorkerRequestHandler {
  constructor({ service, authenticationToken }) {
    if (typeof authenticationToken !== "string" || authenticationToken.length < 16 || authenticationToken.length > 128 || /\s/.test(authenticationToken)) {
      throw new WorkerError("InvalidAuthenticationConfiguration", "Worker authentication token must contain 16 through 128 non-whitespace characters.");
    }
    this.service = service;
    this.authenticationToken = authenticationToken;
  }

  async handle(request) {
    const requestId = typeof request?.requestId === "string" ? request.requestId : "unknown";
    try {
      validateEnvelope(request, this.authenticationToken);
      const payload = request.payload && typeof request.payload === "object" ? request.payload : {};
      let result;
      switch (request.operation) {
        case "worker.capabilities": result = this.service.capabilities(); break;
        case "job.estimate": result = this.service.estimate(payload.request); break;
        case "job.submit": result = await this.service.submit(payload.request); break;
        case "job.get": result = { job: await this.service.get(payload.jobId) }; break;
        case "job.output.read": result = await this.service.readOutput(payload.jobId, payload.outputIndex); break;
        case "job.media.retain": result = await this.service.retainMedia(payload.jobId, payload.outputIndex); break;
        case "job.list": result = { jobs: await this.service.list() }; break;
        case "job.cancel": result = { job: await this.service.cancel(payload.jobId) }; break;
        case "job.retry": result = await this.service.retry(payload.jobId, payload.retryIdempotencyKey, payload.reason, payload.spendApproval); break;
        case "job.resume": result = { job: await this.service.resume(payload.jobId) }; break;
        default: throw new WorkerError("UnknownOperation", `Unknown worker operation: ${request.operation}.`, { remedy: "Use an operation returned by worker.capabilities." });
      }
      return { protocol: PROTOCOL_NAME, version: PROTOCOL_VERSION, requestId, ok: true, result: redactValue(result) };
    } catch (error) {
      return { protocol: PROTOCOL_NAME, version: PROTOCOL_VERSION, requestId, ok: false, error: redactValue(structuredError(error)) };
    }
  }
}

