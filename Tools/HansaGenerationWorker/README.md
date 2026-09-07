# Hansa Generation Worker

`Tools/HansaGenerationWorker` is the external development-only process for provider authentication, long-running generation work, downloads, hashes, manifests, and recovery. It has no Unreal dependency, provider SDK, or credential in the repository. The deterministic `mock` provider is always available. S12-P03 adds an optional dependency-free OpenAI Responses API adapter that is advertised only when its credential, pinned model, and rate-card inputs are injected into the worker environment.

## Run

Node.js 22 or newer is required. Use a fresh local pipe name and a 16–128 character token shared only with the editor process:

```powershell
$env:HANSA_GENERATION_WORKER_PIPE = "hansa-generation-local"
$env:HANSA_GENERATION_WORKER_TOKEN = "replace-with-a-short-lived-random-token"
npm --prefix Tools/HansaGenerationWorker start
```

The default store is `Saved/GenerationJobs`, which is ignored by source control. `--jobs-root=<absolute path>` and `HANSA_GENERATION_JOBS_ROOT` are intended for tests and controlled local environments.

## Protocol

The local transport is a Windows named pipe by default. Each message is a four-byte unsigned little-endian payload length followed by one UTF-8 JSON document, bounded to 1 MiB. Envelopes require protocol `hansa.generation.worker`, version `1.0`, a request ID, the short-lived token, an operation, and an object payload.

Operations are:

- `worker.capabilities`
- `job.estimate` (read-only preflight; no spend approval or durable job)
- `job.submit`
- `job.get`
- `job.list`
- `job.cancel`
- `job.retry`
- `job.resume`
- `job.output.read` (Review-only, hash-verified bounded JSON artifact read)

The checked-in request, normalized-result, manifest, and protocol schemas are under `schemas/`. Errors always contain `code`, safe `message`, `remedy`, and `retryable`; stack traces and raw provider responses never cross the protocol.

## Provider-neutral contract

Adapters implement `getCapabilities`, `validateRequest`, `estimateCost`, `submit`, `poll`, `cancel`, `download`, and `normalizeMetadata`. Jobs select Hansa capabilities such as `StructuredDataDraft`, `TextToMesh`, or `TextToSoundEffect`; provider task IDs remain provenance and never become gameplay identities.

Submission requires a pinned provider/model, stable intended role, idempotency key, bounded prompt, input rights declarations, cost/output budgets, output contract, timeout, and explicit spend approval. Spend approval remains separate from later promotion approval.

The deterministic mock advertises `StructuredDataDraft` on pinned model `mock-v1`. For DefinitionPatch it uses allowlisted current values to emit up to two schema-valid bounded numeric changes, enabling the complete zero-credit acceptance demo. Its parameters can also inject delayed completion, malformed output, timeout, or a revision-bounded transient failure for tests. It performs no network access and spends no credits.

The optional `openai` adapter accepts only `StructuredDataDraft` requests whose intended role is `DefinitionPatch`. It uses a deterministic schema-derived proposal contract, strict Responses API Structured Outputs, `store: false`, stable-reference allowlists, and recorded-response tests. See `Docs/Development/OpenAIDefinitionProposals.md` for setup and the Editor review flow.

## Persistence and recovery

Each job uses this ignored layout:

```text
Saved/GenerationJobs/<JobId>/
├── job.json                       mutable durable state
├── request-r<N>.json              immutable sanitized request revision
├── events.ndjson                  append-only redacted transitions
├── private/input-<N>.bin          raw transient input, never in manifest/log
├── output/artifact-<N>.<ext>      normalized content-addressed result
└── manifests/manifest-r<N>.json   immutable terminal manifest
```

JSON updates use a recoverable temporary/backup swap compatible with Windows. Startup finds `Queued`, `Running`, `Downloading`, `ImportedToStaging`, and `Validating` jobs and resumes the same attempt and provider task. Submit, retry, and resume are idempotent. Retry requires a fresh identified spend approval, increments revision/attempt, and preserves terminal lineage and the prior immutable manifest.

Worker success stops at `Review`. Later editor work owns validation presentation, explicit approval, Unreal staging import, and promotion.

## Redaction and credentials

Credential-like request keys are rejected. The worker token is read only from the environment. Logs and manifests recursively redact credential fields, bearer values, URLs, marked-private prompts, private paths, and inline input bytes. Manifests retain stable hashes, rights declarations, pinned provider/model, budgets, usage, QA, retry lineage, and review placeholders.

## Tests

```powershell
./Scripts/RunGenerationWorkerTests.ps1
```

The contract suite covers success, output/input hashes, immutable manifests, malformed output, timeout, cancellation, retry lineage, process-state restart recovery, idempotent submit/resume, authenticated framing, and credential/private-input redaction. It uses the mock provider plus an injected recorded OpenAI response; it never performs a live provider call.

Run the complete editor/worker mock acceptance paths with:

```powershell
pwsh -NoProfile -File Scripts/RunOpenAIAuthoringAcceptance.ps1
pwsh -NoProfile -File Scripts/RunMediaAcceptance.ps1
```

Both runners require `LiveProviderCalls=false` in their retained result. They prove bounded proposal review and test-only media promotion/reload without provider credentials or spend; they do not substitute for human approval of production media or the Shipping cooked-package audit.
