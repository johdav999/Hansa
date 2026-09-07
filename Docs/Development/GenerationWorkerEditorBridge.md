# Generation worker editor bridge

S12-P02 connects the Editor-only Hansa Authoring Studio to the external `Tools/HansaGenerationWorker` process. Runtime and Shipping modules do not depend on the bridge, worker, provider adapters, credentials, staging files, or job manifests.

## Boundary and transport

`FHansaGenerationWorkerNamedPipeTransport` lives under `Source/HansaEditor/Private/Generation/`. It implements protocol `hansa.generation.worker` version `1.0` over the worker's bounded four-byte little-endian framed Windows named pipe. Every response must match the request ID, protocol name, and version. Overlapped reads and writes share a three-second response deadline; a stalled or partial response cancels pending I/O and returns a retryable error. Connecting has a separate three-second limit. Structured worker errors are presented as safe code, message, remedy, and retryability fields; request bodies and credentials are never logged.

The Editor reads the pipe name from `HANSA_GENERATION_WORKER_PIPE`, defaulting to `hansa-generation-worker-v1`. Authentication resolves in this order:

1. `HANSA_GENERATION_WORKER_TOKEN` inherited by both Editor and worker;
2. a Windows Generic Credential named `Hansa/GenerationWorker` whose blob is the 16–128 character token.

There is no Unreal config, asset, manifest, command-line, or log fallback for the token. The token is added only to the in-memory protocol envelope and cleared from the temporary `FString` after serialization. Provider credentials remain entirely inside the external worker's approved environment or OS credential-store integration.

## Editor responsiveness and request ownership

The panel executes pipe operations and input hashing on background controller snapshots. Slate reads the previous snapshot while work is pending, disables conflicting panel actions, and publishes completion through its active timer on the editor thread. UObject schema construction and proposal review stay on the editor thread. Background jobs capture neither the widget nor UObjects; closing the tab leaves worker-owned persisted jobs intact and does not retain the panel. Controller snapshots share a thread-safe transport owner.

Active jobs refresh once per second after the previous operation completes. Refresh failures stop polling and present the reconnect remedy. An unchanged selected job retains its proposal review during progress refreshes.

## Authoring Studio workflow

Open **Window → Hansa Authoring Studio → Generation jobs**. The full-width native Slate workspace has three columns:

- **Request and approvals** shows the connected provider, provider-neutral capability, pinned model, adapter, input/output media types, prompt, intended role, exact upload preview, rights acknowledgement, hard cost/output limits, read-only estimate, identified approver, and spend confirmation.
- **Job queue** is a virtualized worker list with status text, progress, message, selection, cancellation, and retry. Retry requires a fresh identified spend confirmation.
- **Result and provenance** shows staged output paths, byte counts, SHA-256 hashes, deterministic QA, actual and estimated spend, provider/model/adapter/task provenance, request hash, manifest hash, and structured errors.

Adding files loads at most 16 bounded inputs, computes each SHA-256, and displays project-relative path, role, media type, byte count, and shortened hash. Estimation reloads and rehashes the exact bytes. Submission repeats that check and fails if any file changed after preview. The worker receives bytes only after the operator reviews the list and acknowledges rights.

`job.estimate` is a read-only protocol operation. It normalizes the same provider-neutral request, validates the selected adapter and hard budget, and returns the estimate plus request/input hashes without creating a job or accepting spend approval. Queueing is enabled only when the exact estimated request is unchanged and the operator explicitly approves spend. `job.submit` still rejects unapproved requests, and `job.retry` still requires a fresh approval. Estimates fail closed for missing, negative, fractional, over-budget or wrong-currency amounts and malformed request hashes. Dispatch consumes the estimate even if the response is lost: refresh the queue before deciding whether another submission is needed. Retry consent identifies the selected job and displays its stored estimate and hard cap; a new-request estimate cannot authorize a different job's retry. Selecting another job or replacing inputs clears the corresponding consent.

Worker results stop at `Review`. The panel does not write production assets, approve a draft, or promote staged files. Those remain separate diff, validation, approval, and promotion actions as required by `Docs/EditorArchitecture.md`.

## Definition proposal review

S12-P03 derives an OpenAI proposal contract from the currently selected schema and definition. The proposal estimate and submission therefore bind to the exact schema hash, stable ID, authored revision, content hash, writable fields, stable-reference allowlist, and output bound. Completed JSON is read only from a Review-state, hash-verified worker artifact.

The result panel shows field-level before/after JSON with selective acceptance. Validation applies the selected fields to a transient clone and compiles a temporary registry, exposing before/after hashes without mutating accepted content. Apply repeats the stale checks and validation, changes the asset in one transaction, increments its authored revision, refreshes its content hash, and supports normal Undo/Redo. See [OpenAIDefinitionProposals.md](OpenAIDefinitionProposals.md).
## Validation

The deterministic Node mock covers read-only estimate, authenticated pipe framing, success, malformed output, timeout, cancellation, retry, restart recovery, idempotency, and redaction:

```powershell
pwsh -NoProfile -File Scripts/RunGenerationWorkerTests.ps1
```

Editor automation uses an in-memory provider-neutral transport mock:

```powershell
pwsh -NoProfile -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Architecture.GenerationWorker
```

The tests prove exact upload metadata and SHA-256, estimate-before-approval gating, request-change invalidation, credential-free editor payloads, fresh retry approval, cancellation routing, staged outputs, QA, and provenance parsing. Regression tests cover changed file bytes, invalid cost/currency estimates, a lost submit response, an actual stalled local named pipe, editor-thread completion, and closing the panel during background work.

Shipping exclusion remains a separate acceptance gate:

```powershell
pwsh -NoProfile -File Scripts/VerifyShippingExclusion.ps1 -Platform Win64
```

The audit rejects `HansaEditor` and `HansaGenerationWorker` tokens in the Shipping target receipt or executable and verifies that runtime build rules have no reverse dependency on Editor/DeveloperTool modules. Its current scope is receipt and executable; the later packaged/cooked/depot audit remains the release gate described by the architecture.

## Visual assets

The component inventory and four accepted 1536×1024 built-in ImageGen references are under `Docs/Images/UI/GenerationJobs/`; source masters and sibling prompt records are under `SourceArt/UI/GenerationJobs/`. They are non-shipping references. No raster was imported into `Content/`, scaled, cropped, or used by the native Slate implementation.
