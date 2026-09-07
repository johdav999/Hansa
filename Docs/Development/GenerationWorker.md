# S12-P01 generation worker protocol and job store

`Tools/HansaGenerationWorker` implements the external worker boundary defined by `EditorArchitecture.md`. It is a Node.js 22 development tool rather than an Unreal module, so runtime targets cannot link it and packaged builds do not stage it.

The version 1.0 authenticated local protocol uses bounded length-prefixed JSON over a named pipe. Capability discovery and submit/get/list/cancel/retry/resume operations return provider-neutral documents and structured errors. The first adapter is a deterministic, network-free mock for ordinary CI.

Jobs default to ignored `Saved/GenerationJobs/<JobId>` directories. State updates use recoverable Windows-safe swaps; sanitized request revisions and terminal manifests are immutable; events are append-only. Startup reconciles fully persisted jobs back into the idempotency index if a crash interrupted the final index update, preventing an orphaned job or duplicate provider submission. Raw input bytes remain only in the transient private directory. Public records carry SHA-256 request, input, output, and manifest hashes.

The worker persists the full Draft → Estimated → ApprovedToSpend → Queued → Running → Downloading → ImportedToStaging → Validating → Review path. Failed, Cancelled, and Expired are terminal for an attempt. Explicit retry requires a new spend approval and creates a new revision and attempt under the same stable job identity while retaining retry lineage and earlier manifests. Startup resumes recoverable states with the prior provider task ID, and repeated submit/retry/resume commands are idempotent.

Credential-shaped fields are rejected at request validation. URLs, bearer values, marked-private prompts, private paths, and private input contents do not enter logs, protocol job views, request records, or manifests. Provider credentials belong only in the worker environment or a later OS credential-store adapter.

Run `./Scripts/RunGenerationWorkerTests.ps1` for artifacted evidence or `npm --prefix Tools/HansaGenerationWorker test` directly. The suite uses no live provider and covers every S12-P01 failure and recovery case.

## S12-P03 OpenAI proposal adapter

The optional OpenAI Responses API adapter is enabled only by complete worker-environment configuration. It accepts schema-derived, revision-bound DefinitionPatch requests, sends a strict non-retained JSON Schema request, and rejects prose, stale identity, unknown fields/references, and schema mismatches before Review. job.output.read exposes only one bounded, hash-verified JSON artifact to the Editor review surface. See [OpenAIDefinitionProposals.md](OpenAIDefinitionProposals.md).


## S12-P04 acceptance flow

The deterministic mock now emits a strict DefinitionPatch from the contract's allowlisted current values, changing up to two bounded numeric recipe or technology fields for the zero-credit acceptance flow. The full secure setup, model/rate-card pinning, budget policy, manual review/apply/Undo/Redo sequence, and restart recovery procedure are documented in [OpenAIAuthoringAcceptance.md](OpenAIAuthoringAcceptance.md).

## S13-P01 retained media

The authenticated job.media.retain operation exports completed media sources and immutable provenance into SourceArt. Native import, review and atomic create-only promotion are described in [StagedMedia.md](StagedMedia.md).


## S13-P02 static harbor props

The Tripo v3 adapter, canonical HarborProp profile, native normalization/collision QA, deterministic capture and explicit spend/promotion workflow are documented in [TripoStaticProps.md](TripoStaticProps.md). Generic retained media keeps the existing v1 contract; Tripo adds a required versioned static-prop profile.

## S13-P03 audio takes

The ElevenLabs adapters, versioned audio profile, decode/PCM QA, stable subtitles, native playback and approval workflow are documented in [ElevenLabsAudioTakes.md](ElevenLabsAudioTakes.md). [S13-P03 evidence](Evidence/S13P03-20260906.md) records automated results and remaining manual/release gates.

## S13-P04 connected acceptance

[MediaAcceptance.md](MediaAcceptance.md) documents the mock-only real-process recovery runner, native promotion/reload proof, clean manual demo and optional budgeted live smoke. Run `Scripts/RunMediaAcceptance.ps1` for aggregate evidence.
