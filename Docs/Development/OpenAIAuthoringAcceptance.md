# OpenAI authoring acceptance flow

S12-P04 proves one bounded recipe proposal from request through review, simulation preview, transactional apply, Undo/Redo, and deterministic registry recompile. The default walkthrough uses the network-free `mock-v1` provider and spends no credits. OpenAI is an optional replacement for the submit step.

## Secure worker setup

From the repository root, choose one literal pipe name and a random 16-128 character session token. Set the same two values in two PowerShell windows. Start the worker in the first:

```powershell
$env:HANSA_GENERATION_WORKER_PIPE = "hansa-generation-local"
$env:HANSA_GENERATION_WORKER_TOKEN = "<random-session-token>"
npm --prefix Tools/HansaGenerationWorker start
```

Start Unreal Editor in the second:

```powershell
$env:HANSA_GENERATION_WORKER_PIPE = "hansa-generation-local"
$env:HANSA_GENERATION_WORKER_TOKEN = "<the-same-random-session-token>"
& "H:\Unreal\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\Johan\source\repos\Unreal\Hansa\Hansa\Hansa.uproject"
```

Do not put the token or any provider key in `.ini`, assets, requests, logs, manifests, shell scripts, or source control. The Editor may alternatively read the local worker token from the Windows Generic Credential named `Hansa/GenerationWorker`.

For a live OpenAI run, inject these additional values only into the worker environment:

```text
OPENAI_API_KEY
HANSA_OPENAI_MODEL
HANSA_OPENAI_RATE_CARD_ID
HANSA_OPENAI_INPUT_MICRODOLLARS_PER_MILLION
HANSA_OPENAI_OUTPUT_MICRODOLLARS_PER_MILLION
```

Set `HANSA_OPENAI_MODEL` to the exact reviewed model or snapshot identifier. Record a new rate-card ID whenever either injected rate changes. Use a dedicated low-privilege key, a small request budget, and the proposal contract's output-token ceiling. Normal CI and the acceptance test never make a live provider call.

## Manual zero-credit demo

1. Start the worker with the secure setup above, leaving the OpenAI variables unset.
2. Open **Window > Hansa Authoring Studio**, choose the **Data** workspace, and select `Recipe.MillFlour`.
3. Open **Generation jobs**. Keep provider `mock` and pinned model `mock-v1`; choose `StructuredDataDraft` and intended role `DefinitionPatch`.
4. Enter: `Propose one bounded balance adjustment for Recipe.MillFlour. Change cycle time and labor only; preserve stable IDs and references.`
5. Set the cost ceiling to zero, keep the bounded JSON output contract, acknowledge the input-rights declaration, and run **Estimate**. The estimate must report zero cost.
6. Enter the local approver identity and submit. Wait for state **Review**, select the job, and choose **Load reviewed JSON**.
7. In the field diff, clear the checkbox for `LaborerWorkforce` and leave `CycleTicks` selected. This rejects one field and accepts the other.
8. Choose **Validate + preview fixture**. Confirm that the temporary definition and registry are valid. Inspect the current and proposed baseline, shortage, and recovery stock, price, unmet-demand, state-hash, and recovery-contract results for `lubeck_grain_shortage_v1`.
9. Choose **Apply selected fields**. Verify that `CycleTicks` changes, `LaborerWorkforce` does not, and authored revision increments once.
10. Use **Undo**, then **Redo**. Use the Data workspace registry validation/compile action to compare hashes: Undo returns to the current registry hash and Redo matches the proposed preview hash. Do not use an old proposal Apply action to replay an accepted change.

For an explicitly approved live run, choose provider `openai`, its injected pinned model, and a maximum cost at or above the fresh estimate. The field review, fixture preview, apply, and Undo/Redo steps remain the same.

## Failure and recovery drills

| Failure | Expected result | Recovery |
|---|---|---|
| Definition changes after proposal creation | `StaleProposal` or `StaleOrMismatchedProposal`; apply stays disabled | Discard the artifact, rebuild the contract from the current revision, estimate again, and submit a new proposal |
| Unknown stable reference | `UnknownStableReference` before Review/apply | Correct the referenced definition or request a new proposal from the current reference allowlist |
| Protected field such as `StableDefinitionId` | `ForbiddenProposalField` | Remove the protected field; only schema fields marked `Suggest` or `Generate` may enter the patch |
| Prose, unknown keys, missing keys, or malformed JSON | `MalformedProviderOutput`, `UnknownProposalField`, or `SchemaMismatch` | Inspect the safe structured error and retry only after correcting the provider/contract issue |
| User cancellation | Job reaches `Cancelled` and records an immutable attempt manifest | Submit a new job if work is still wanted |
| Retryable provider failure | Job reaches `Failed` with a retryable error | Select the failed job, review its stored estimate and hard cap, enter the approver and check the fresh retry confirmation; prior attempt evidence remains immutable |
| Worker process stops mid-job | Durable job remains recoverable | Restart with the same jobs root, pipe configuration, and provider configuration; startup resumes the same provider task ID idempotently |
| Fixture preview fails | `FixturePreviewFailed`; apply stays disabled | Restore the canonical MVP fixture dependencies or reject incompatible fields |

## Automated evidence

Run the complete network-free acceptance gate:

```powershell
pwsh -NoProfile -File Scripts/RunOpenAIAuthoringAcceptance.ps1
```

Use `-SkipBuild` only when the Editor binaries match the source. `-EngineRoot` and `-ArtifactsRoot` are supported. Close the Editor before rebuilding its loaded DLL. `Scripts/InvokeCI.ps1` runs this gate even when a narrower general test filter is selected.

The gate performs:

1. the mock/recorded worker contract suite;
2. the native recipe acceptance test and export of its real schema-derived contract;
3. submission of that contract to a fresh persistent mock worker, recovery from the queued state after reopening the store, completion to Review, and hash-verified artifact reading;
4. a second native acceptance run using the actual worker JSON rather than a constructed proposal;
5. the full GenerationWorker editor architecture test filter;
6. a combined `result.json` that requires the final editor source to be `persistent-mock-worker` and the worker to report no live calls.

The worker driver imports only `DeterministicMockProvider`; ambient OpenAI credentials cannot enable an adapter. Existing recorded-response tests inject fetch mocks. The runner restores its test-only process environment variables even on failure.

Evidence is written beneath ignored `Saved/BuildArtifacts/<run>-openai-authoring-acceptance/`:

- `editor-contract.json`: real exported writable schema, selected base revision/hash and reference allowlist;
- `worker-proposal.json`: verified staged worker output used by the second Editor run;
- `worker-acceptance.json` and `jobs/`: provider/model provenance, artifact hash, immutable manifest and recovery evidence;
- `editor-acceptance.json`: selected/rejected fields, stale-apply rejection, Undo/Redo, and current/proposed baseline, shortage and recovery metrics with explicit units and hashes;
- `result.json`: successful combined gate with nested worker/editor evidence;
- per-stage Unreal and Node logs/results.

## Failure coverage map

| Required case | Automated evidence |
| --- | --- |
| Stale content | Recipe acceptance attempts Apply after a concurrent edit and verifies rejection preserves that edit; architecture tests also reject stale loads/revisions |
| Invalid references | Recipe acceptance rejects unknown nested goods; `ProposalArrayReferences` rejects unknown recipe array entries |
| Forbidden fields | Recipe acceptance rejects identity changes; worker tests reject unknown/root/inherited keys |
| Malformed response | Recipe acceptance rejects malformed envelopes; recorded adapter tests reject prose, schema mismatch and invalid numeric/usage data |
| Cancellation | Worker suite cancels both polling and in-flight submission, verifies terminal state and immutable evidence |
| Worker restart | Round-trip driver reopens a queued native-contract job and completes it once; worker suite additionally proves in-flight process-crash recovery and idempotent resume |
| Review/preview/apply | The actual worker proposal rejects workforce, accepts cycle ticks, compares grain-shortage metrics, applies once, undoes/redoes and matches deterministic registry hashes |

The manual walkthrough is supplied for operator use; headless automation proves data and transactions, not visual layout, keyboard navigation or a live-provider demo. The Shipping exclusion gate remains separate. See [S12-P04 verification](Evidence/S12P04-20260906.md) for this run's evidence and limitations.
