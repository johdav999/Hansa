# ADR-0012: External generation worker protocol and persistent store

- **Status:** Accepted
- **Date:** 2026-09-06

## Context

Hansa needs resumable OpenAI and media-generation jobs without putting provider SDKs, credentials, downloads, or long-running work in Unreal. The editor must survive worker restarts and future providers must fit one Hansa-level job model.

## Decision

`Tools/HansaGenerationWorker` is a Node.js 22 external development process. It uses authenticated version 1.0 length-prefixed JSON over a local named pipe and exposes provider-neutral capabilities plus read-only estimate, submit, get, list, cancel, retry, and resume operations. The estimate operation validates and hashes the exact request without creating durable state or accepting spend approval.

Job state is stored as per-job JSON beneath ignored `Saved/GenerationJobs`. Mutable state uses recoverable temporary/backup replacement. Sanitized request revisions and terminal manifests are immutable files; events are append-only. Raw input bytes stay in a private transient subdirectory. SHA-256 identifies requests, inputs, outputs, and manifests.

Provider adapters implement capability discovery, validation, estimation, submission, polling, cancellation, download, and normalized provenance. Provider IDs and task IDs remain workflow provenance. Hansa stable IDs remain the only gameplay identity.

Job success ends at Review. Unreal staging import, human approval, and promotion remain editor-owned actions. Spend approval and promotion approval are separate.

## Consequences

Worker and editor restarts can resume the same provider attempt without duplicate submission. Retry preserves earlier terminal evidence in a new job revision. Normal CI can exercise the complete lifecycle with a deterministic mock and no credits.

Transient job directories may contain private source bytes needed for a resumable request, but public job views, logs, sanitized requests, and manifests contain only safe descriptors and hashes. Credentials are rejected in request documents. The worker uses its environment or provider-specific approved OS credential integration; the Unreal bridge resolves its local authentication token from the shared worker environment or Windows Generic Credential `Hansa/GenerationWorker`.

## Compliance evidence

`Tools/HansaGenerationWorker/tests/worker-contract.test.js` covers read-only estimation, success, malformed output, timeout, cancellation, retry, restart, idempotent resume, authenticated framing, and redaction. Mock-backed `Hansa.Architecture.GenerationWorker` Editor automation covers exact upload preview, approval gates, queue actions, safe payloads, and provenance parsing. `Scripts/RunGenerationWorkerTests.ps1` records results beneath ignored `Saved/BuildArtifacts`.

## Deferred choices

Tripo, ElevenLabs, and TRELLIS adapter details, webhook transports, provider credential-store adapters, staging import, and promotion are handled by later sprint slices without changing this job identity or state-machine boundary.


## 2026-09-06 clarification: bounded OpenAI definition proposals

The first live-capable adapter uses the OpenAI Responses API only inside the external worker. It is opt-in through complete environment configuration, uses an explicitly pinned model and injected rate card, sends `store: false`, and requests strict JSON Schema output. Definition proposal identity binds to the deterministic exported-schema hash plus the selected definition's stable ID, authored revision, and content hash. Only fields classified `Suggest` or `Generate` and known stable references enter the contract.

Provider output remains a draft. A Review-only bounded artifact read feeds an Editor field diff. Selective acceptance is validated on a transient clone and temporary registry, then applied through one explicit Unreal transaction. Recorded responses cover normal CI; live calls require a separate explicit, budgeted operator action.


## 2026-09-06 clarification: acceptance and balance preview

Definition proposal contracts include the current JSON value of every allowlisted writable field. This gives adapters bounded context while excluding protected properties. Recipe and technology reviews execute lubeck_grain_shortage_v1 against both compiled registries and expose typed phase metrics before apply. The deterministic mock supports the full zero-credit review flow; automated evidence covers selective acceptance, invalid references, forbidden fields, malformed output, cancellation, restart recovery, one-transaction apply, Undo/Redo, and deterministic recompile.
