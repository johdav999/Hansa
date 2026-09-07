export const PROTOCOL_NAME = "hansa.generation.worker";
export const PROTOCOL_VERSION = "1.0";
export const JOB_SCHEMA_VERSION = 1;
export const MANIFEST_SCHEMA_VERSION = 1;
export const RESULT_CONTRACT_VERSION = 1;
export const RESULT_V1_FIELDS = Object.freeze([
  "resultContractVersion",
  "artifacts",
  "usage",
  "providerJobId",
  "contractHash",
  "proposalHash",
  "model",
]);
export const MAX_FRAME_BYTES = 1024 * 1024;

export const JobStatus = Object.freeze({
  Draft: "Draft",
  Estimated: "Estimated",
  ApprovedToSpend: "ApprovedToSpend",
  Queued: "Queued",
  Running: "Running",
  Downloading: "Downloading",
  ImportedToStaging: "ImportedToStaging",
  Validating: "Validating",
  Review: "Review",
  Approved: "Approved",
  Promoted: "Promoted",
  Rejected: "Rejected",
  RevisionRequested: "RevisionRequested",
  Failed: "Failed",
  Cancelled: "Cancelled",
  Expired: "Expired",
});

export const TERMINAL_WORKER_STATUSES = new Set([
  JobStatus.Review,
  JobStatus.Failed,
  JobStatus.Cancelled,
  JobStatus.Expired,
]);

export const RECOVERABLE_STATUSES = new Set([
  JobStatus.Queued,
  JobStatus.Running,
  JobStatus.Downloading,
  JobStatus.ImportedToStaging,
  JobStatus.Validating,
]);

export const ALLOWED_TRANSITIONS = new Map([
  [JobStatus.Draft, new Set([JobStatus.Estimated, JobStatus.Cancelled, JobStatus.Failed])],
  [JobStatus.Estimated, new Set([JobStatus.ApprovedToSpend, JobStatus.Cancelled, JobStatus.Failed])],
  [JobStatus.ApprovedToSpend, new Set([JobStatus.Queued, JobStatus.Cancelled, JobStatus.Failed])],
  [JobStatus.Queued, new Set([JobStatus.Running, JobStatus.Cancelled, JobStatus.Expired, JobStatus.Failed])],
  [JobStatus.Running, new Set([JobStatus.Downloading, JobStatus.Cancelled, JobStatus.Expired, JobStatus.Failed])],
  [JobStatus.Downloading, new Set([JobStatus.ImportedToStaging, JobStatus.Cancelled, JobStatus.Expired, JobStatus.Failed])],
  [JobStatus.ImportedToStaging, new Set([JobStatus.Validating, JobStatus.Cancelled, JobStatus.Failed])],
  [JobStatus.Validating, new Set([JobStatus.Review, JobStatus.Cancelled, JobStatus.Failed])],
  [JobStatus.Review, new Set([JobStatus.Approved, JobStatus.Rejected, JobStatus.RevisionRequested])],
  [JobStatus.Approved, new Set([JobStatus.Promoted, JobStatus.Rejected])],
  [JobStatus.RevisionRequested, new Set([JobStatus.Estimated, JobStatus.Cancelled])],
  [JobStatus.Failed, new Set([JobStatus.Estimated])],
  [JobStatus.Cancelled, new Set([JobStatus.Estimated])],
  [JobStatus.Expired, new Set([JobStatus.Estimated])],
]);
