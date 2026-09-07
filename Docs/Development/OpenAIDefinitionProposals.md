# OpenAI definition proposals

S12-P03 adds an optional OpenAI Responses API adapter to the external `Tools/HansaGenerationWorker` process. Unreal runtime modules never load the adapter or receive its credential. The adapter is advertised only when every required worker environment value is present:

```text
OPENAI_API_KEY
HANSA_OPENAI_MODEL
HANSA_OPENAI_RATE_CARD_ID
HANSA_OPENAI_INPUT_MICRODOLLARS_PER_MILLION
HANSA_OPENAI_OUTPUT_MICRODOLLARS_PER_MILLION
```

`HANSA_OPENAI_MODEL` is an explicitly pinned model identifier. The two rates are integral microdollars per million input/output tokens. The worker uses them for the existing preflight spend approval and records `HANSA_OPENAI_RATE_CARD_ID` with provenance. The conservative input bound counts every UTF-8 byte of the complete serialized Responses request (including developer instructions, schema, contract and envelope), plus 1,024 tokens of framing allowance; the output bound comes from the proposal contract. Configuration is injected into the worker process and must not be checked into Unreal config, content, requests, logs, or manifests.

## Bounded contract

The Authoring Studio derives `parameters.definitionProposal` from the selected definition and `FHansaEditorSchemaRegistry::ExportJsonSchema`. The contract includes the schema ID/version and SHA-256, stable definition ID, authored revision, deterministic content hash, the exact `Suggest`/`Generate` field schemas and current values, an ordinal stable-reference allowlist, and a bounded output-token limit. Attached files are rejected before estimation because this adapter consumes only schema-derived definition context; its advertised input media list is empty. Current values are limited to already allowlisted fields so the provider can suggest a contextual bounded change without receiving protected identity or workflow fields. Changing the definition or schema changes the estimate identity, so prior spend approval no longer applies.

The adapter sends `store: false` and a strict `text.format` JSON Schema to `/v1/responses`. Every writable patch property is required and nullable: `null` means leave the field unchanged. Hansa-only schema annotations remain in local validation and are removed from the provider schema. Returned data must be exactly one JSON object. Response reads are streamed with an envelope allowance of 256 KiB above the artifact budget and a hard 4 MiB ceiling. HTTP submission and body reads share the remaining job deadline and cancellation signal; redirects are rejected. Returned response ID, exact configured model, and nonnegative integral token usage are checked before staging. The worker rejects prose, unknown fields, missing fields, wrong schema/base identity, stale revision or content hash, out-of-range or unsafe integers, unknown stable references, and an all-null patch before creating the review artifact.

The normalized artifact contains only the proposal JSON. The manifest provenance records provider, pinned model, adapter version, provider job ID, rate-card ID, proposal hash, contract hash, usage, and `store: false`. Provider responses and credentials are not persisted.

## Authoring review

A completed proposal remains in worker state `Review`. In **Window → Hansa Authoring Studio → Generation jobs**, select the job and choose **Load reviewed JSON**. The panel reads one size- and hash-verified `application/json` artifact through `job.output.read`, rechecks schema/base identity, and shows one row per non-null field with before/after JSON and an independent acceptance checkbox. Native review independently checks exact JSON types, integer precision, metadata ranges, nested field membership, and array reference IDs before conversion. Fractional schema/base revisions cannot be truncated into valid identities. Review owns a deep copy so later mutation of the input JSON cannot change the displayed proposal.

**Validate + preview fixture** clones the selected definition, applies only checked fields, runs definition validation, and compiles the temporary registry to show before/after definition and registry hashes. Recipe and technology proposals also execute `lubeck_grain_shortage_v1` against the current and proposed registries and show baseline, shortage, and recovery metrics, state hashes, and contract results. **Apply selected fields** repeats the stale check and validation, writes the chosen fields in one Unreal transaction, increments authored revision once, refreshes the content hash, and marks the asset dirty. Normal Undo/Redo restores the whole apply.

## Tests

Normal tests never call OpenAI:

```powershell
pwsh -NoProfile -File Scripts\RunGenerationWorkerTests.ps1
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Architecture.GenerationWorker
```

The Node suite uses an injected fetch implementation and a checked-in recorded response. It verifies the request shape, credential isolation, structured-output validation, rejection paths, provenance, and adapter opt-in. The Unreal suite verifies contract derivation, selective field review, temporary-registry compilation, transactional apply/undo, stale rejection, forbidden fields, and bounded artifact reads. Any live smoke remains a separate explicit operator action with a dedicated low-privilege key and approved budget; it is not part of CI or these scripts.


## S12-P04 acceptance

The network-free manual walkthrough, secure live setup, budget and pinning policy, recovery drills, and automated failure-coverage map are in [OpenAIAuthoringAcceptance.md](OpenAIAuthoringAcceptance.md).


## S12-P03 verification

See [S12P03-20260906.md](Evidence/S12P03-20260906.md) for the tested source hashes, 20 passing worker tests, nine passing editor architecture tests, the passing selective-apply/fixture/Undo/Redo acceptance flow, and the Shipping audit scope.

The Responses `text.format` strict JSON Schema shape was checked against the [official Structured Outputs guide](https://developers.openai.com/api/docs/guides/structured-outputs). No live OpenAI request was made.
