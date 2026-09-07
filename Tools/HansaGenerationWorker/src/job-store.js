import { randomUUID } from "node:crypto";
import { appendFile, lstat, mkdir, readFile, readdir, rename, rm, stat, writeFile } from "node:fs/promises";
import path from "node:path";
import { ALLOWED_TRANSITIONS, JOB_SCHEMA_VERSION, JobStatus, MANIFEST_SCHEMA_VERSION } from "./contracts.js";
import { canonicalJson, hashDocument, sha256 } from "./canonical-json.js";
import { WorkerError } from "./errors.js";
import { redactValue } from "./redaction.js";

async function atomicWriteJson(filePath, value) {
  const temporary = `${filePath}.${process.pid}.${randomUUID()}.tmp`;
  const backup = `${filePath}.bak`;
  await writeFile(temporary, `${canonicalJson(value)}\n`, { encoding: "utf8", flag: "wx", flush: true });
  try {
    await rename(temporary, filePath);
    return;
  } catch (error) {
    if (error?.code !== "EEXIST" && error?.code !== "EPERM") {
      await rm(temporary, { force: true });
      throw error;
    }
  }
  await rm(backup, { force: true });
  try {
    await rename(filePath, backup);
  } catch (error) {
    if (error?.code !== "ENOENT") {
      await rm(temporary, { force: true });
      throw error;
    }
  }
  try {
    await rename(temporary, filePath);
    await rm(backup, { force: true });
  } catch (error) {
    try { await rename(backup, filePath); } catch {}
    await rm(temporary, { force: true });
    throw error;
  }
}

async function readJsonWithRecovery(filePath) {
  try {
    return JSON.parse(await readFile(filePath, "utf8"));
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
    try {
      return JSON.parse(await readFile(`${filePath}.bak`, "utf8"));
    } catch (backupError) {
      if (backupError?.code !== "ENOENT") throw backupError;
      return JSON.parse(await readFile(filePath, "utf8"));
    }
  }
}
async function writeImmutableJson(filePath, value) {
  const text = `${canonicalJson(value)}\n`;
  try {
    await writeFile(filePath, text, { encoding: "utf8", flag: "wx" });
  } catch (error) {
    if (error?.code !== "EEXIST") throw error;
    const existing = await readFile(filePath, "utf8");
    if (existing !== text) {
      throw new WorkerError("ImmutableRecordConflict", "An immutable job record already exists with different content.", {
        remedy: "Preserve the existing job directory and submit a new idempotency key.",
      });
    }
  }
}

function artifactExtension(mediaType) {
  const known = new Map([["application/json", "json"], ["text/plain", "txt"], ["model/gltf-binary", "glb"], ["audio/wav", "wav"], ["audio/mpeg", "mp3"]]);
  return known.get(mediaType) ?? "bin";
}

export class JobStore {
  constructor(rootDirectory, { logger, clock = () => new Date() } = {}) {
    this.rootDirectory = path.resolve(rootDirectory);
    this.logger = logger;
    this.clock = clock;
    this.indexPath = path.join(this.rootDirectory, "idempotency-index.json");
    this.lock = Promise.resolve();
  }

  now() {
    return this.clock().toISOString();
  }

  jobDirectory(jobId) {
    if (!/^[0-9a-f-]{36}$/i.test(jobId)) throw new WorkerError("InvalidJobId", "jobId is invalid.");
    return path.join(this.rootDirectory, jobId);
  }

  async initialize() {
    await mkdir(this.rootDirectory, { recursive: true });
    try {
      await stat(this.indexPath);
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
      await atomicWriteJson(this.indexPath, { schemaVersion: 1, submissions: {} });
    }
    await this.#reconcileIndex();
  }

  async #reconcileIndex() {
    const index = await this.#readIndex();
    if (index?.schemaVersion !== 1 || !index.submissions || typeof index.submissions !== "object" || Array.isArray(index.submissions)) {
      throw new WorkerError("CorruptJobStore", "The generation job idempotency index is invalid.", {
        remedy: "Preserve Saved/GenerationJobs for diagnosis and restore a valid index or backup.",
      });
    }

    let changed = false;
    const entries = await readdir(this.rootDirectory, { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isDirectory() || !/^[0-9a-f-]{36}$/i.test(entry.name)) continue;
      const job = await readJsonWithRecovery(path.join(this.rootDirectory, entry.name, "job.json"));
      const keyHash = job?.request?.idempotencyKeyHash;
      if (job?.jobId !== entry.name || !/^[0-9a-f]{64}$/.test(keyHash ?? "")) {
        throw new WorkerError("CorruptJobStore", "Generation job directory " + entry.name + " does not contain a valid persisted identity.", {
          remedy: "Preserve the job directory for diagnosis before repairing or removing it.",
        });
      }
      const indexedJobId = index.submissions[keyHash];
      if (indexedJobId && indexedJobId !== job.jobId) {
        throw new WorkerError("IdempotencyIndexConflict", "Two persisted generation jobs claim the same idempotency key.", {
          remedy: "Preserve both job directories and resolve the conflicting records before restarting the worker.",
        });
      }
      if (!indexedJobId) {
        index.submissions[keyHash] = job.jobId;
        changed = true;
      }
    }
    if (changed) await atomicWriteJson(this.indexPath, index);
  }

  async #exclusive(action) {
    const current = this.lock.then(action, action);
    this.lock = current.catch(() => undefined);
    return current;
  }

  async #readIndex() {
    return readJsonWithRecovery(this.indexPath);
  }

  async createJob(normalized, estimate) {
    return this.#exclusive(async () => {
      const index = await this.#readIndex();
      const existingJobId = index.submissions[normalized.idempotencyKeyHash];
      if (existingJobId) {
        const existing = await this.getJob(existingJobId);
        if (existing.requestHash !== normalized.requestHash) {
          throw new WorkerError("IdempotencyConflict", "The submission idempotency key is already bound to a different request.", {
            remedy: "Reuse the key only for the identical request or submit a new idempotency key.",
          });
        }
        return { job: existing, created: false };
      }

      const jobId = randomUUID();
      const directory = this.jobDirectory(jobId);
      await Promise.all([
        mkdir(path.join(directory, "private"), { recursive: true }),
        mkdir(path.join(directory, "output"), { recursive: true }),
        mkdir(path.join(directory, "manifests"), { recursive: true }),
      ]);
      const inputArtifacts = [];
      for (const descriptor of normalized.safeRequest.inputArtifacts) {
        const privateInput = normalized.privateInputs.find((item) => item.index === descriptor.index);
        const privateRelativePath = `private/input-${String(descriptor.index + 1).padStart(3, "0")}.bin`;
        await writeFile(path.join(directory, privateRelativePath), privateInput.bytes, { flag: "wx" });
        inputArtifacts.push({ ...descriptor, privateRelativePath });
      }
      const request = { ...normalized.safeRequest, inputArtifacts };
      const now = this.now();
      const job = {
        schemaVersion: JOB_SCHEMA_VERSION,
        jobId,
        parentJobId: null,
        revision: 1,
        attempt: 1,
        capability: request.capability,
        intendedAssetRole: request.intendedAssetRole,
        provider: { providerId: request.providerId, modelVersion: request.modelVersion },
        request,
        requestHash: normalized.requestHash,
        inputHash: normalized.inputHash,
        status: JobStatus.Draft,
        progress: { phase: JobStatus.Draft, percent: 0, message: "Job draft persisted." },
        timestamps: { createdAt: now, updatedAt: now, startedAt: null, completedAt: null },
        deadlineAt: new Date(this.clock().getTime() + request.timeoutMs).toISOString(),
        estimate,
        actualUsage: null,
        retryLineage: [],
        retryKeys: [],
        resumeCount: 0,
        cancellationRequested: false,
        providerJobId: null,
        providerState: null,
        providerResult: null,
        provenance: null,
        errors: [],
        outputs: [],
        qaResults: [],
        manifests: [],
      };
      await writeImmutableJson(path.join(directory, "request-r1.json"), this.safeRequestRecord(job));
      await atomicWriteJson(path.join(directory, "job.json"), job);
      await this.appendEvent(job, "job.created", { status: job.status, requestHash: job.requestHash, inputHash: job.inputHash });
      index.submissions[normalized.idempotencyKeyHash] = jobId;
      await atomicWriteJson(this.indexPath, index);
      return { job, created: true };
    });
  }

  safeRequestRecord(job) {
    return {
      schemaVersion: 1,
      jobId: job.jobId,
      revision: job.revision,
      requestHash: job.requestHash,
      inputHash: job.inputHash,
      request: {
        ...job.request,
        inputArtifacts: job.request.inputArtifacts.map(({ privateRelativePath: _privateRelativePath, ...item }) => item),
      },
    };
  }

  async getJob(jobId) {
    try {
      return await readJsonWithRecovery(path.join(this.jobDirectory(jobId), "job.json"));
    } catch (error) {
      if (error?.code === "ENOENT") throw new WorkerError("JobNotFound", `Generation job ${jobId} was not found.`);
      throw error;
    }
  }

  async listJobs() {
    const index = await this.#readIndex();
    const ids = [...new Set(Object.values(index.submissions))];
    const jobs = await Promise.all(ids.map((id) => this.getJob(id)));
    return jobs.sort((left, right) => left.timestamps.createdAt.localeCompare(right.timestamps.createdAt));
  }

  async updateJob(jobId, mutator, event = "job.updated") {
    return this.#exclusive(async () => {
      const job = await this.getJob(jobId);
      const beforeStatus = job.status;
      await mutator(job);
      job.timestamps.updatedAt = this.now();
      await atomicWriteJson(path.join(this.jobDirectory(jobId), "job.json"), job);
      await this.appendEvent(job, event, { beforeStatus, status: job.status, progress: job.progress });
      return job;
    });
  }

  async transition(jobId, nextStatus, patch = {}, event = "job.transitioned") {
    return this.updateJob(jobId, (job) => {
      if (job.status === nextStatus) Object.assign(job, patch);
      else {
        const allowed = ALLOWED_TRANSITIONS.get(job.status);
        if (!allowed?.has(nextStatus)) {
          throw new WorkerError("InvalidJobTransition", `Cannot transition generation job from ${job.status} to ${nextStatus}.`);
        }
        job.status = nextStatus;
        Object.assign(job, patch);
      }
      job.progress = patch.progress ?? { phase: nextStatus, percent: job.progress?.percent ?? 0, message: nextStatus };
      if (nextStatus === JobStatus.Running && !job.timestamps.startedAt) job.timestamps.startedAt = this.now();
      if ([JobStatus.Review, JobStatus.Failed, JobStatus.Cancelled, JobStatus.Expired].includes(nextStatus)) job.timestamps.completedAt = this.now();
    }, event);
  }

  async prepareRetry(jobId, retryKeyHash, reason, spendApproval) {
    return this.#exclusive(async () => {
      const job = await this.getJob(jobId);
      if (job.retryKeys.includes(retryKeyHash)) return { job, retried: false };
      if (![JobStatus.Failed, JobStatus.Cancelled, JobStatus.Expired].includes(job.status)) {
        throw new WorkerError("JobNotRetryable", `Job ${jobId} cannot be retried from ${job.status}.`);
      }
      const prior = { revision: job.revision, attempt: job.attempt, status: job.status, errors: job.errors, completedAt: job.timestamps.completedAt };
      job.retryLineage.push(prior);
      job.retryKeys.push(retryKeyHash);
      job.request.spendApproval = spendApproval;
      job.requestHash = hashDocument({ ...job.request, inputArtifacts: job.request.inputArtifacts.map(({ privateRelativePath: _privateRelativePath, ...item }) => item) });
      job.revision += 1;
      job.attempt += 1;
      job.status = JobStatus.Draft;
      job.progress = { phase: JobStatus.Draft, percent: 0, message: "Retry draft persisted." };
      job.timestamps.updatedAt = this.now();
      job.timestamps.startedAt = null;
      job.timestamps.completedAt = null;
      job.deadlineAt = new Date(this.clock().getTime() + job.request.timeoutMs).toISOString();
      job.cancellationRequested = false;
      job.providerJobId = null;
      job.providerState = null;
      job.providerResult = null;
      job.provenance = null;
      job.actualUsage = null;
      job.errors = [];
      job.outputs = [];
      job.qaResults = [];
      await writeImmutableJson(path.join(this.jobDirectory(jobId), `request-r${job.revision}.json`), this.safeRequestRecord(job));
      await atomicWriteJson(path.join(this.jobDirectory(jobId), "job.json"), job);
      await this.appendEvent(job, "job.retried", { reason, retryRevision: job.revision, prior });
      return { job, retried: true };
    });
  }

  async writeArtifacts(job, artifacts) {
    if (!Array.isArray(artifacts) || artifacts.length === 0 || artifacts.length > job.request.outputContract.maximumArtifacts) {
      throw new WorkerError("MalformedProviderOutput", "Provider result contains an invalid artifact count.", { retryable: true });
    }
    let totalBytes = 0;
    const outputs = [];
    for (let index = 0; index < artifacts.length; index += 1) {
      const artifact = artifacts[index];
      if (!artifact || typeof artifact.logicalName !== "string" || !job.request.outputContract.allowedMediaTypes.includes(artifact.mediaType) || typeof artifact.contentBase64 !== "string") {
        throw new WorkerError("MalformedProviderOutput", `Provider artifact ${index} violates the output contract.`, { retryable: true });
      }
      const bytes = Buffer.from(artifact.contentBase64, "base64");
      if (bytes.toString("base64").replace(/=+$/, "") !== artifact.contentBase64.replace(/=+$/, "")) {
        throw new WorkerError("MalformedProviderOutput", `Provider artifact ${index} is not canonical base64.`, { retryable: true });
      }
      totalBytes += bytes.length;
      if (totalBytes > job.request.budgets.maximumOutputBytes) {
        throw new WorkerError("OutputBudgetExceeded", "Provider output exceeds maximumOutputBytes.", { retryable: false });
      }
      const revisionDirectory = job.revision === 1 ? "output" : `output/r${job.revision}`;
      await mkdir(path.join(this.jobDirectory(job.jobId), revisionDirectory), { recursive: true });
      const relativePath = `${revisionDirectory}/artifact-${String(index + 1).padStart(3, "0")}.${artifactExtension(artifact.mediaType)}`;
      const outputPath = path.join(this.jobDirectory(job.jobId), relativePath);
      try {
        await writeFile(outputPath, bytes, { flag: "wx" });
      } catch (error) {
        if (error?.code !== "EEXIST" || sha256(await readFile(outputPath)) !== sha256(bytes)) throw error;
      }
      outputs.push({ logicalName: artifact.logicalName, mediaType: artifact.mediaType, sizeBytes: bytes.length, sha256: sha256(bytes), relativePath });
    }
    return outputs;
  }

  async retainProviderSource(jobId, index, bytes, mediaType) {
    const job = await this.getJob(jobId);
    if (!Number.isInteger(index) || index < 0 || index >= job.request.outputContract.maximumArtifacts ||
        index > 1 || mediaType !== "audio/mpeg" || !Buffer.isBuffer(bytes) || !bytes.length || bytes.length > 2 * 1024 * 1024)
      throw new WorkerError("ProviderSourceRejected", "Invalid bounded original audio take.");
    const directory = job.revision === 1 ? "output" : `output/r${job.revision}`;
    await mkdir(path.join(this.jobDirectory(jobId), directory), { recursive: true });
    const relativePath = directory + `/original-${String(index + 1).padStart(3, "0")}.mp3`;
    const file = path.join(this.jobDirectory(jobId), relativePath);
    try { await writeFile(file, bytes, { flag: "wx", flush: true }); }
    catch (error) { if (error.code !== "EEXIST" || sha256(await readFile(file)) !== sha256(bytes)) throw error; }
    return { relativePath, mediaType, sizeBytes: bytes.length, sha256: sha256(bytes) };
  }

  async readPrivateInput(jobId, index) {
    const job = await this.getJob(jobId);
    const input = job.request.inputArtifacts[index];
    if (!Number.isInteger(index) || index < 0 || !input || input.index !== index ||
        input.privateRelativePath !== `private/input-${String(index + 1).padStart(3, "0")}.bin`)
      throw new WorkerError("InputIntegrityFailed", "Invalid private input identity.");
    const directory = this.jobDirectory(jobId);
    for (const entry of [directory, path.join(directory, "private"), path.join(directory, input.privateRelativePath)])
      if ((await lstat(entry)).isSymbolicLink()) throw new WorkerError("InputIntegrityFailed", "Private inputs cannot follow links.");
    const file = path.join(directory, input.privateRelativePath);
    const info = await lstat(file);
    if (!info.isFile() || info.size !== input.sizeBytes || info.size > 6 * 1024 * 1024)
      throw new WorkerError("InputIntegrityFailed", "Private input size changed.");
    const bytes = await readFile(file);
    if (sha256(bytes) !== input.sha256) throw new WorkerError("InputIntegrityFailed", "Private input hash changed.");
    return bytes;
  }

  async readOutput(jobId, outputIndex) {
    if (!Number.isSafeInteger(outputIndex) || outputIndex < 0 || outputIndex > 15) {
      throw new WorkerError("InvalidRequest", "outputIndex must be an integer from 0 through 15.");
    }
    const job = await this.getJob(jobId);
    if (job.status !== JobStatus.Review) {
      throw new WorkerError("OutputNotReady", "Generation output can be read only after deterministic validation reaches Review.");
    }
    const output = job.outputs[outputIndex];
    if (!output || output.mediaType !== "application/json" || !new RegExp(`^output/${job.revision === 1 ? "" : `r${job.revision}/`}artifact-[0-9]{3}\\.json$`).test(output.relativePath)) {
      throw new WorkerError("OutputUnavailable", "The selected output is not a bounded JSON proposal artifact.");
    }
    const bytes = await readFile(path.join(this.jobDirectory(jobId), output.relativePath));
    if (bytes.length !== output.sizeBytes || sha256(bytes) !== output.sha256 || bytes.length > 1024 * 1024) {
      throw new WorkerError("OutputIntegrityFailed", "The staged proposal no longer matches its recorded size and SHA-256.");
    }
    let document;
    try { document = JSON.parse(bytes.toString("utf8")); }
    catch { throw new WorkerError("MalformedProviderOutput", "The staged proposal artifact is not valid JSON."); }
    if (!document || typeof document !== "object" || Array.isArray(document)) {
      throw new WorkerError("MalformedProviderOutput", "The staged proposal artifact must contain one JSON object.");
    }
    return { jobId, outputIndex, sha256: output.sha256, document };
  }
  async writeManifest(job, normalizedProvenance) {
    const manifestWithoutHash = {
      schemaVersion: MANIFEST_SCHEMA_VERSION,
      protocolVersion: "1.0",
      jobId: job.jobId,
      parentJobId: job.parentJobId,
      revision: job.revision,
      attempt: job.attempt,
      capability: job.capability,
      intendedAssetRole: job.intendedAssetRole,
      provider: job.provider,
      providerJobId: job.providerJobId,
      requestHash: job.requestHash,
      inputHash: job.inputHash,
      prompt: job.request.prompt,
      promptSha256: job.request.promptSha256,
      negativePrompt: job.request.negativePrompt,
      inputArtifacts: job.request.inputArtifacts.map(({ privateRelativePath: _privateRelativePath, ...item }) => item),
      rights: job.request.rights,
      budgets: job.request.budgets,
      estimate: job.estimate,
      actualUsage: job.actualUsage,
      outputContract: job.request.outputContract,
      seed: job.request.seed,
      parameters: job.request.parameters,
      status: job.status,
      progress: job.progress,
      timestamps: job.timestamps,
      retryLineage: job.retryLineage,
      outputs: job.outputs,
      errors: job.errors,
      qaResults: job.qaResults,
      provenance: normalizedProvenance,
      review: { reviewer: null, decision: "Pending", destination: null, promotedRevision: null },
    };
    const manifestHash = hashDocument(manifestWithoutHash);
    const manifest = { ...manifestWithoutHash, manifestHash };
    const relativePath = `manifests/manifest-r${job.revision}.json`;
    await writeImmutableJson(path.join(this.jobDirectory(job.jobId), relativePath), manifest);
    return { relativePath, manifestHash };
  }

  async appendEvent(job, event, details = {}) {
    const safe = redactValue({ timestamp: this.now(), event, jobId: job.jobId, revision: job.revision, ...details });
    await appendFile(path.join(this.jobDirectory(job.jobId), "events.ndjson"), `${JSON.stringify(safe)}\n`, "utf8");
    this.logger?.log("info", event, { jobId: job.jobId, revision: job.revision, status: job.status });
  }
}
