import { JobStatus, RECOVERABLE_STATUSES, RESULT_CONTRACT_VERSION, RESULT_V1_FIELDS, TERMINAL_WORKER_STATUSES } from "./contracts.js";
import { hashDocument, sha256 } from "./canonical-json.js";
import { WorkerError, structuredError } from "./errors.js";
import { normalizeGenerationRequest } from "./request-contract.js";
import { assertProviderContract } from "./providers/mock-provider.js";
import { retainMediaSource } from "./media-source.js";

function publicJob(job) {
  const { providerResult: _providerResult, providerState: _providerState, retryKeys: _retryKeys, ...safe } = job;
  return {
    ...safe,
    request: {
      ...safe.request,
      inputArtifacts: safe.request.inputArtifacts.map(({ privateRelativePath: _privateRelativePath, ...item }) => item),
    },
  };
}

function abortableDelay(milliseconds, signal) {
  return new Promise((resolve, reject) => {
    if (signal.aborted) return reject(signal.reason ?? new Error("aborted"));
    const onAbort = () => {
      clearTimeout(timer);
      reject(signal.reason ?? new Error("aborted"));
    };
    const timer = setTimeout(() => {
      signal.removeEventListener("abort", onAbort);
      resolve();
    }, milliseconds);
    signal.addEventListener("abort", onAbort, { once: true });
  });
}

function validateProviderResult(result, job) {
  if (!result || typeof result !== "object" || Array.isArray(result) || result.resultContractVersion !== RESULT_CONTRACT_VERSION || !Array.isArray(result.artifacts) || !result.usage) {
    throw new WorkerError("MalformedProviderOutput", "Provider output does not match result contract version 1.", {
      remedy: "Inspect the provider adapter and retry only after correcting its normalized result.",
      retryable: true,
    });
  }
  const unknownFields = Object.keys(result).filter((field) => !RESULT_V1_FIELDS.includes(field));
  if (unknownFields.length > 0) {
    throw new WorkerError("MalformedProviderOutput", "Provider result contains unknown fields: " + unknownFields.join(", ") + ".", { retryable: true });
  }
  if (result.artifacts.length < 1 || result.artifacts.length > 16) {
    throw new WorkerError("MalformedProviderOutput", "Provider result must contain between 1 and 16 artifacts.", { retryable: true });
  }
  const artifactFields = new Set(["logicalName", "mediaType", "contentBase64"]);
  for (const artifact of result.artifacts) {
    if (!artifact || typeof artifact !== "object" || Array.isArray(artifact) || Object.keys(artifact).some((field) => !artifactFields.has(field))
      || typeof artifact.logicalName !== "string" || artifact.logicalName.length < 1 || artifact.logicalName.length > 128
      || typeof artifact.mediaType !== "string" || !/^[a-z0-9.+-]+\/[a-z0-9.+-]+$/.test(artifact.mediaType)
      || typeof artifact.contentBase64 !== "string") {
      throw new WorkerError("MalformedProviderOutput", "Provider artifact does not match result contract version 1.", { retryable: true });
    }
  }
  if (typeof result.providerJobId !== "string" || result.providerJobId.length < 1 || result.providerJobId.length > 256) {
    throw new WorkerError("MalformedProviderOutput", "Provider result must include a bounded providerJobId.", { retryable: true });
  }
  for (const field of ["contractHash", "proposalHash"]) {
    if (result[field] !== undefined && !/^[0-9a-f]{64}$/.test(result[field])) {
      throw new WorkerError("MalformedProviderOutput", "Provider result " + field + " must be a lowercase SHA-256 hash.", { retryable: true });
    }
  }
  if (result.model !== undefined && (typeof result.model !== "string" || result.model.length < 1 || result.model.length > 256)) {
    throw new WorkerError("MalformedProviderOutput", "Provider result model must be a bounded string when present.", { retryable: true });
  }
  const usageFields = new Set(["currency", "actualMinorUnits", "usageUnit", "actualUsage"]);
  if (typeof result.usage !== "object" || Array.isArray(result.usage) || Object.keys(result.usage).some((field) => !usageFields.has(field))) {
    throw new WorkerError("MalformedProviderOutput", "Provider usage contains fields outside result contract version 1.", { retryable: true });
  }
  const actual = result.usage.actualMinorUnits;
  if (!/^[A-Z]{3}$/.test(result.usage.currency ?? "") || !Number.isSafeInteger(actual) || actual < 0 || typeof result.usage.usageUnit !== "string" || typeof result.usage.actualUsage !== "number" || !Number.isFinite(result.usage.actualUsage) || result.usage.actualUsage < 0) {
    throw new WorkerError("MalformedProviderOutput", "Provider usage does not match result contract version 1.", { retryable: true });
  }
  if (result.usage.currency !== job.request.budgets.currency || actual > job.request.budgets.maximumCostMinorUnits) {
    throw new WorkerError("CostBudgetExceeded", "Provider usage exceeded the approved job budget or changed currency.", { retryable: false });
  }
}

export class GenerationWorkerService {
  constructor({ store, providers, logger, pollIntervalMs = 20, autoProcess = true, projectRoot } = {}) {
    this.store = store;
    this.projectRoot = projectRoot;
    this.providers = new Map();
    for (const provider of providers ?? []) {
      assertProviderContract(provider);
      this.providers.set(provider.providerId, provider);
    }
    this.logger = logger;
    this.pollIntervalMs = pollIntervalMs;
    this.autoProcess = autoProcess;
    this.active = new Map();
  }

  async initialize() {
    await this.store.initialize();
    const jobs = await this.store.listJobs();
    for (const job of jobs) {
      if (RECOVERABLE_STATUSES.has(job.status)) {
        await this.store.updateJob(job.jobId, (saved) => {
          saved.resumeCount += 1;
          saved.progress = { ...saved.progress, message: `Recovered ${saved.status} job after worker restart.` };
        }, "job.recovered");
        if (this.autoProcess) this.schedule(job.jobId);
      } else if (TERMINAL_WORKER_STATUSES.has(job.status) && !job.manifests.some((item) => item.revision === job.revision)) {
        const provider = this.provider(job.provider.providerId);
        const provenance = job.provenance ?? {
          providerId: job.provider.providerId,
          modelVersion: job.provider.modelVersion,
          adapterVersion: provider.adapterVersion,
          providerJobId: job.providerJobId,
        };
        const manifest = await this.store.writeManifest(job, provenance);
        await this.store.updateJob(job.jobId, (saved) => { saved.manifests.push({ ...manifest, revision: saved.revision }); }, "job.manifest-recovered");
      }
    }
  }

  capabilities() {
    return {
      protocolVersion: "1.0",
      providers: [...this.providers.values()].map((provider) => provider.getCapabilities()),
      operations: ["worker.capabilities", "job.estimate", "job.submit", "job.get", "job.output.read", "job.list", "job.cancel", "job.retry", "job.resume", ...(this.projectRoot ? ["job.media.retain"] : [])],
      persistentState: true,
      immutableManifests: true,
    };
  }


  estimate(rawRequest) {
    const normalized = normalizeGenerationRequest(rawRequest, { requireSpendApproval: false });
    const provider = this.provider(normalized.safeRequest.providerId);
    provider.validateRequest(normalized.safeRequest);
    const estimate = provider.estimateCost(normalized.safeRequest);
    if (!estimate || estimate.currency !== normalized.safeRequest.budgets.currency || !Number.isSafeInteger(estimate.estimatedMinorUnits) || estimate.estimatedMinorUnits < 0) {
      throw new WorkerError("InvalidCostEstimate", "The provider returned an invalid cost estimate.");
    }
    if (estimate.estimatedMinorUnits > normalized.safeRequest.budgets.maximumCostMinorUnits) {
      throw new WorkerError("CostBudgetExceeded", "Estimated provider cost exceeds the requested maximum budget.", {
        remedy: "Increase the explicit budget or select a lower-cost request before approval.",
      });
    }
    return { estimate, requestHash: normalized.requestHash, inputHash: normalized.inputHash };
  }
  provider(providerId) {

    const provider = this.providers.get(providerId);
    if (!provider) {
      throw new WorkerError("ProviderUnavailable", `Provider ${providerId} is unavailable.`, {
        remedy: "Choose a provider returned by worker.capabilities.",
      });
    }
    return provider;
  }

  async submit(rawRequest) {
    const normalized = normalizeGenerationRequest(rawRequest);
    const provider = this.provider(normalized.safeRequest.providerId);
    provider.validateRequest(normalized.safeRequest);
    const estimate = provider.estimateCost(normalized.safeRequest);
    if (!estimate || estimate.currency !== normalized.safeRequest.budgets.currency || !Number.isSafeInteger(estimate.estimatedMinorUnits) || estimate.estimatedMinorUnits < 0) {
      throw new WorkerError("InvalidCostEstimate", "The provider returned an invalid cost estimate.");
    }
    if (estimate.estimatedMinorUnits > normalized.safeRequest.budgets.maximumCostMinorUnits) {
      throw new WorkerError("CostBudgetExceeded", "Estimated provider cost exceeds the approved job budget.", {
        remedy: "Increase the explicit budget or select a lower-cost request before resubmitting.",
      });
    }
    const { job, created } = await this.store.createJob(normalized, estimate);
    if (created) {
      await this.store.transition(job.jobId, JobStatus.Estimated, {
        estimate,
        progress: { phase: JobStatus.Estimated, percent: 0, message: "Provider-neutral cost estimate recorded." },
      });
      await this.store.transition(job.jobId, JobStatus.ApprovedToSpend, {
        progress: { phase: JobStatus.ApprovedToSpend, percent: 0, message: "Explicit spend approval recorded separately from promotion approval." },
      });
      await this.store.transition(job.jobId, JobStatus.Queued, {
        progress: { phase: JobStatus.Queued, percent: 0, message: "Job queued for provider submission." },
      });
    }
    if (this.autoProcess) this.schedule(job.jobId);
    return { job: publicJob(await this.store.getJob(job.jobId)), created };
  }

  async get(jobId) {
    return publicJob(await this.store.getJob(jobId));
  }

  async readOutput(jobId, outputIndex) {
    return this.store.readOutput(jobId, outputIndex);
  }

  async retainMedia(jobId, outputIndex) {
    return retainMediaSource(this.store, this.projectRoot, jobId, outputIndex);
  }

  async list() {
    return Promise.all((await this.store.listJobs()).map(publicJob));
  }

  async cancel(jobId) {
    let job = await this.store.getJob(jobId);
    if (TERMINAL_WORKER_STATUSES.has(job.status)) return publicJob(job);
    job = await this.store.updateJob(jobId, (saved) => {
      saved.cancellationRequested = true;
      saved.progress = { ...saved.progress, message: "Cancellation requested." };
    }, "job.cancellation-requested");
    // A synchronous HTTP submission may not have a provider task ID yet.
    if (!job.providerJobId || this.provider(job.provider.providerId).requiresSubmissionGuard) this.active.get(jobId)?.controller.abort(new Error("User cancellation"));
    if (this.autoProcess) this.schedule(jobId);
    return publicJob(job);
  }

  async retry(jobId, retryIdempotencyKey, reason = "Explicit retry", spendApproval) {
    if (typeof retryIdempotencyKey !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(retryIdempotencyKey)) {
      throw new WorkerError("InvalidRequest", "retryIdempotencyKey is invalid.");
    }
    if (!spendApproval || spendApproval.approved !== true || typeof spendApproval.approvedBy !== "string" || !spendApproval.approvedBy || typeof spendApproval.approvedAt !== "string" || !spendApproval.approvedAt) {
      throw new WorkerError("SpendApprovalRequired", "Retry requires a new explicit spend approval.", {
        remedy: "Review the retry estimate and include an identified retry spend approval.",
      });
    }
    const safeApproval = {
      approved: true,
      approvedBy: spendApproval.approvedBy.slice(0, 128),
      approvedAt: spendApproval.approvedAt.slice(0, 64),
    };
    const previous = await this.store.getJob(jobId);
    const retryProvider = this.provider(previous.provider.providerId);
    retryProvider.validateRequest(previous.request);
    const retryEstimate = retryProvider.estimateCost(previous.request);
    if (!retryEstimate || retryEstimate.currency !== previous.request.budgets.currency ||
        !Number.isSafeInteger(retryEstimate.estimatedMinorUnits) || retryEstimate.estimatedMinorUnits < 0 ||
        retryEstimate.estimatedMinorUnits > previous.request.budgets.maximumCostMinorUnits)
      throw new WorkerError("CostBudgetExceeded", "Retry estimate exceeds the original approved cost ceiling.");
    const { job, retried } = await this.store.prepareRetry(jobId, sha256(retryIdempotencyKey), String(reason).slice(0, 512), safeApproval);
    if (retried) {
      const provider = this.provider(job.provider.providerId);
      const estimate = provider.estimateCost(job.request);
      await this.store.transition(jobId, JobStatus.Estimated, { estimate, progress: { phase: JobStatus.Estimated, percent: 0, message: "Retry estimate recorded." } });
      await this.store.transition(jobId, JobStatus.ApprovedToSpend, { progress: { phase: JobStatus.ApprovedToSpend, percent: 0, message: "New explicit retry spend approval recorded." } });
      await this.store.transition(jobId, JobStatus.Queued, { progress: { phase: JobStatus.Queued, percent: 0, message: "Retry queued." } });
    }
    if (this.autoProcess) this.schedule(jobId);
    return { job: publicJob(await this.store.getJob(jobId)), retried };
  }

  async resume(jobId) {
    const job = await this.store.getJob(jobId);
    if (RECOVERABLE_STATUSES.has(job.status) && this.autoProcess) this.schedule(jobId);
    return publicJob(job);
  }

  schedule(jobId) {
    if (this.active.has(jobId)) return this.active.get(jobId).promise;
    const controller = new AbortController();
    const promise = this.#run(jobId, controller.signal)
      .catch((error) => this.#handleRunError(jobId, error, controller.signal))
      .finally(() => this.active.delete(jobId));
    this.active.set(jobId, { controller, promise });
    return promise;
  }

  async #handleRunError(jobId, error, signal) {
    if (signal.aborted) {
      const cancelled = await this.store.getJob(jobId);
      if (cancelled.cancellationRequested && !TERMINAL_WORKER_STATUSES.has(cancelled.status)) {
        const provider = this.provider(cancelled.provider.providerId);
        if (cancelled.providerJobId) {
          const state = await provider.cancel(cancelled.providerJobId, cancelled.providerState);
          await this.store.updateJob(jobId, saved => { saved.providerState = state; }, "job.provider-cancelled");
        }
        await this.#finish(jobId, JobStatus.Cancelled, { code: "CancelledByUser", message: provider.cancellationMessage ?? "The generation request was cancelled.", retryable: true,
          ...(error instanceof WorkerError && error.details ? { details: error.details } : {}) });
      }
      return;
    }
    this.logger?.log("error", "job.execution-failed", { jobId, error: structuredError(error) });
    const job = await this.store.getJob(jobId);
    if (TERMINAL_WORKER_STATUSES.has(job.status)) return;
    await this.#finish(jobId, JobStatus.Failed, structuredError(error));
  }

  async #run(jobId, signal) {
    let job = await this.store.getJob(jobId);
    const provider = this.provider(job.provider.providerId);

    if (job.cancellationRequested) {
      if (job.providerJobId) job.providerState = await provider.cancel(job.providerJobId, job.providerState);
      await this.#finish(jobId, JobStatus.Cancelled, {
        code: "CancelledByUser", message: provider.cancellationMessage ?? "The generation job was cancelled.", remedy: "Retry with a new explicit action if the work is still needed.", retryable: true,
      });
      return;
    }

    if (job.status === JobStatus.Queued) {
      job = await this.store.transition(jobId, JobStatus.Running, {
        progress: { phase: JobStatus.Running, percent: 1, message: "Submitting provider-neutral request." },
      });
    }

    if (job.status === JobStatus.Running) {
      if (!job.providerJobId) {
        if (Date.now() >= Date.parse(job.deadlineAt)) {
          await this.#finish(jobId, JobStatus.Expired, { code: "ProviderTimeout", message: "Job expired before submission.", retryable: true });
          return;
        }
        if (provider.requiresSubmissionGuard) {
          if (job.submissionIntentRevision === job.revision)
            throw new WorkerError("SubmissionOutcomeUnknown", "An earlier billable submission may have reached the provider. Inspect the provider account before explicitly approving another spend.", { retryable: true });
          job = await this.store.updateJob(jobId, saved => { saved.submissionIntentRevision = saved.revision; }, "job.submission-intent");
        }
        const submitted = await provider.submit(job.request, {
          jobId, revision: job.revision, attempt: job.attempt, idempotencyKeyHash: job.request.idempotencyKeyHash,
          readInput: index => this.store.readPrivateInput(jobId, index),
          retainProviderSource: (index, bytes, mediaType) => this.store.retainProviderSource(jobId, index, bytes, mediaType),
          signal, remainingTimeoutMs: Math.max(1, Date.parse(job.deadlineAt) - Date.now()),
        });
        if (!submitted?.providerJobId || !submitted.state) {
          throw new WorkerError("MalformedProviderOutput", "Provider submit did not return a task ID and resumable state.", { retryable: true });
        }
        job = await this.store.updateJob(jobId, (saved) => {
          saved.providerJobId = submitted.providerJobId;
          saved.providerState = submitted.state;
          saved.progress = { phase: JobStatus.Running, percent: 5, message: "Provider task submitted." };
        }, "job.provider-submitted");
      }

      while (job.status === JobStatus.Running) {
        if (signal.aborted) throw signal.reason;
        job = await this.store.getJob(jobId);
        if (job.cancellationRequested) {
          const cancelledState = await provider.cancel(job.providerJobId, job.providerState);
          await this.store.updateJob(jobId, (saved) => { saved.providerState = cancelledState; }, "job.provider-cancelled");
          await this.#finish(jobId, JobStatus.Cancelled, {
            code: "CancelledByUser", message: provider.cancellationMessage ?? "The generation job was cancelled.", remedy: "Retry with a new explicit action if needed.", retryable: true,
          });
          return;
        }
        if (Date.now() >= Date.parse(job.deadlineAt)) {
          await provider.cancel(job.providerJobId, job.providerState);
          await this.#finish(jobId, JobStatus.Expired, {
            code: "ProviderTimeout", message: "The provider did not complete within the bounded job timeout." + (provider.cancellationMessage ? " " + provider.cancellationMessage : ""), remedy: "Retry or increase the bounded timeout after reviewing provider health.", retryable: true,
          });
          return;
        }
        const polled = await provider.poll(job.providerJobId, job.providerState, { request: job.request, revision: job.revision, attempt: job.attempt, signal, remainingTimeoutMs: Math.max(1, Date.parse(job.deadlineAt) - Date.now()) });
        job = await this.store.updateJob(jobId, (saved) => {
          saved.providerState = polled.state;
          saved.progress = { phase: JobStatus.Running, percent: Number(polled.progress ?? saved.progress.percent), message: `Provider poll: ${polled.status}.` };
        }, "job.provider-polled");
        if (polled.status === "pending") {
          await abortableDelay(Math.max(this.pollIntervalMs, provider.pollIntervalMs ?? 0), signal);
          continue;
        }
        if (polled.status === "cancelled") {
          await this.#finish(jobId, JobStatus.Cancelled, { code: "ProviderCancelled", message: "The provider cancelled the task.", remedy: "Retry if the request remains valid.", retryable: true });
          return;
        }
        if (polled.status === "failed") {
          await this.#finish(jobId, JobStatus.Failed, polled.error ?? { code: "ProviderFailed", message: "The provider failed without a normalized error.", remedy: "Inspect provider health before retrying.", retryable: true });
          return;
        }
        if (polled.status !== "completed") {
          throw new WorkerError("MalformedProviderOutput", `Provider poll returned unknown status ${polled.status}.`, { retryable: true });
        }
        validateProviderResult(polled.result, job);
        job = await this.store.transition(jobId, JobStatus.Downloading, {
          providerResult: polled.result,
          actualUsage: polled.result.usage,
          progress: { phase: JobStatus.Downloading, percent: 96, message: "Downloading normalized provider artifacts." },
        });
      }
    }

    if (job.status === JobStatus.Downloading) {
      validateProviderResult(job.providerResult, job);
      const artifacts = await provider.download(job.providerResult, { jobId, revision: job.revision });
      const outputs = await this.store.writeArtifacts(job, artifacts);
      job = await this.store.transition(jobId, JobStatus.ImportedToStaging, {
        outputs,
        progress: { phase: JobStatus.ImportedToStaging, percent: 98, message: "Artifacts hashed in the isolated transient staging directory." },
      });
    }

    if (job.status === JobStatus.ImportedToStaging) {
      job = await this.store.transition(jobId, JobStatus.Validating, {
        progress: { phase: JobStatus.Validating, percent: 99, message: "Validating hashes and output contract." },
      });
    }

    if (job.status === JobStatus.Validating) {
      const qaResults = [{ check: "OutputContract", passed: true }, { check: "OutputHashes", passed: job.outputs.every((item) => /^[0-9a-f]{64}$/.test(item.sha256)) }];
      if (!qaResults.every((item) => item.passed)) throw new WorkerError("OutputValidationFailed", "Worker output validation failed.");
      const provenance = provider.normalizeMetadata(job.providerResult, { request: job.request, providerState: job.providerState });
      job = await this.store.transition(jobId, JobStatus.Review, {
        qaResults,
        providerResult: null,
        provenance,
        progress: { phase: JobStatus.Review, percent: 100, message: "Worker output is ready for explicit editor review." },
      });
      const manifest = await this.store.writeManifest(job, provenance);
      await this.store.updateJob(jobId, (saved) => { saved.manifests.push({ ...manifest, revision: saved.revision }); }, "job.manifest-written");
    }
  }

  async #finish(jobId, status, error) {
    const current = await this.store.getJob(jobId);
    const provider = this.provider(current.provider.providerId);
    const provenance = {
      providerId: current.provider.providerId,
      modelVersion: current.provider.modelVersion,
      adapterVersion: provider.adapterVersion,
      providerJobId: current.providerJobId,
    };
    const job = await this.store.transition(jobId, status, {
      errors: [error],
      providerResult: null,
      provenance,
      progress: { phase: status, percent: status === JobStatus.Cancelled ? 0 : 100, message: error.message },
    });
    const manifest = await this.store.writeManifest(job, provenance);
    await this.store.updateJob(jobId, (saved) => { saved.manifests.push({ ...manifest, revision: saved.revision }); }, "job.manifest-written");
  }

  async closeForRestart() {
    const tasks = [];
    for (const entry of this.active.values()) {
      entry.controller.abort(new Error("Worker restart"));
      tasks.push(entry.promise);
    }
    await Promise.allSettled(tasks);
  }

  async waitForIdle() {
    await Promise.allSettled([...this.active.values()].map((entry) => entry.promise));
  }
}

export { publicJob };

