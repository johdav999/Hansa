# Tripo static harbor props (S13-P02)

The external worker implements one text/image-to-static-mesh workflow. It uses the existing authenticated job protocol and the S13-P01 retained-source/import/approval pipeline. No provider SDK, credential, HTTP call, or generation job enters runtime modules.

## Supported contract

- Provider: `tripo`; pinned model: `v3.1-20260211`; adapter: `1.0.0`; API: v3.
- Capabilities: `TextToMesh`, `ImageToMesh`; intended role: `HarborProp`.
- One embedded, uncompressed GLB 2 mesh in one identity scene hierarchy; one LOD. FBX is detected/rejected, never passed to Unreal's FBX importer. This workflow explicitly requests triangle GLB output (`quad=false`).
- No skeletal assets, skins, animations, morph targets, extensions, external URI resources, nonidentity node transforms, generated parts, retopology, or TRELLIS calls.
- Required provider-neutral profile: [static-prop-v1.schema.json](../../Tools/HansaGenerationWorker/schemas/static-prop-v1.schema.json). Hansa `Prop.*` identity remains independent of provider task IDs and filenames.
- Profile ceilings: height 10–1000 cm; 1–50,000 triangles; 1–8 materials; texture dimensions 64–4096 pixels; at most four textures per allowed material; maximum source 64 MiB. Choose lower limits for the actual asset.
- Input: one rights-declared PNG/JPEG for ImageToMesh. The local protocol's 1 MiB frame limit also applies to the entire base64 request; use a suitably sized original reference. Do not resample repository raster assets to bypass their asset contracts.

The canonical profile is additive to retained-source v1: it is stored in immutable manifest `parameters.staticProp`, with worker geometry results in `source.json.validation.staticProp`. Existing generic GLB/WAV retained sources remain valid. Tripo sources cannot omit the profile. Changing canonical semantics requires a new profile version and re-staging; changing an asset request requires a new job/revision and review.

## Worker setup and spend

Set credentials only in the external worker's process environment or approved credential launcher. Never put them in project settings, command arguments, request JSON, manifests, screenshots, or Shipping files.

| Variable | Meaning |
| --- | --- |
| `HANSA_TRIPO_ENABLED=1` | Explicitly enable the adapter; otherwise absent from capabilities |
| `TRIPO_API_KEY` | Dedicated provider credential |
| `HANSA_TRIPO_MODEL=v3.1-20260211` | Pinned model |
| `HANSA_TRIPO_ESTIMATED_CREDITS` | Conservative integer credits estimate for this fixed workflow |
| `HANSA_TRIPO_MINOR_UNITS_PER_CREDIT` | Conservative integer currency minor units per provider credit |
| `HANSA_TRIPO_CURRENCY` | Currency code, default USD |
| `HANSA_TRIPO_DOWNLOAD_HOSTS` | Optional comma-separated exact Tripo-domain HTTPS hosts; default cdn.tripo3d.ai |

The two rate-card values must be explicitly configured from the account's current pricing. The adapter does not invent a price. Estimates above the request ceiling fail before submission; observed usage above it fails review and is retained in failure evidence. **This is a local admission limit, not a remote provider account spending cap.** Provider price changes and local cancellation can still incur charges. Use the provider account's own controls for an external spending ceiling.

Start the worker as described in [GenerationWorker.md](GenerationWorker.md), including its local authentication token. Copy [the disabled request template](../../Tools/HansaGenerationWorker/examples/tripo-harbor-prop.json) to a local request file. Set a unique idempotency key, actual rights declaration, approved maximum cost, prompt and prop profile. The template defaults to no rights acknowledgement and zero spend.

```powershell
node Tools/HansaGenerationWorker/scripts/tripo.js estimate <request.json>
node Tools/HansaGenerationWorker/scripts/tripo.js submit <request.json> --approve-spend=<reviewer> --confirm-cost=<estimated-minor-units>
node Tools/HansaGenerationWorker/scripts/tripo.js status <job-id>
node Tools/HansaGenerationWorker/scripts/tripo.js cancel <job-id>
```

The submission command re-estimates, requires an exact confirmed amount and a named approver, then records spend approval independently from content promotion. Image requests change `capability` and add one `inputArtifacts` entry containing role, PNG/JPEG MIME, canonical base64 and rightsDeclaration. Verified private bytes are uploaded through the worker; caller URLs and provider file tokens are not accepted as input identity. Prompts marked private or altered by redaction are rejected because the worker must not send a placeholder as the intended prompt.

## Recovery and cancellation

Task polling is spaced by at least 1.5 seconds and respects the job deadline. Safe GET requests retry 429/selected 5xx up to three attempts. HTTP operations have bounded deadlines, response readers enforce byte ceilings, redirects are rejected, and storage requests never receive the API Authorization header.

Tripo's documented v3 task API has no public cancellation operation. Cancellation aborts local HTTP/poll/download work and records that remote generation and billing may continue. It does not claim to cancel a provider-side task.

Billable POST requests are never automatically retried. A durable submission-intent record is written before submission. After a crash:

- A persisted task ID resumes polling without another generation request.
- A submission intent without a task ID fails with `SubmissionOutcomeUnknown`. Inspect the provider account before approving another spend.
- Signed model URLs are never persisted; polling obtains them in memory. Only validated artifact bytes enter durable result state.
- An explicit retry requires a new spend approval, revalidates the rate card against the original ceiling, and creates a new immutable output revision.

```powershell
node Tools/HansaGenerationWorker/scripts/tripo.js retry <job-id> --approve-spend=<reviewer> --confirm-budget=<original-ceiling> --retry-key=<unique-action-key>
```

Failed native QA requires reviewing the actual geometry/profile and producing a new revision. Do not weaken source hashes or edit a previous approval receipt to make it pass.

## Unreal staging and approval

When the worker reaches Review:

```powershell
node Tools/HansaGenerationWorker/scripts/tripo.js retain <job-id>
```

Use the returned descriptor in the existing Editor console commands:

```text
Hansa.Media.Stage <SourceArt/Generated/.../source.json>
Hansa.Media.Preview <SourceArt/Generated/.../import-....json>
Hansa.Media.Promote <receipt> </Game/Hansa/Meshes/NewHarborProp> <Prop.StableId> <reviewer> "<rights statement>" <review-hash> APPROVE
```

See [StagedMedia.md](StagedMedia.md) for exact command argument handling and create-only promotion semantics.

Native staging imports the retained GLB into an isolated bundle, converts glTF meters/Y-up to Unreal centimeters/Z-up, preserves +X forward, uniformly fits the requested height and bakes a bottom-center pivot. The import receipt records original bounds, subtracted origin, uniform scale and axis conversion. It creates a single matching box collider; triangles, material slots/assets, native texture decode/dimensions, centimeter bounds, pivot and collision are checked again during review and promotion.

Preview uses the existing native asset editors plus a transient texture editor showing a real rendered scene. It has a fixed floor, key/fill lighting, manual exposure, 45-degree camera, fixed relative viewpoint, and native 1280×720 capture. There is no new GUI screen or generated GUI artwork. The capture is retained beside the import receipt and its hash is required for promotion. Mesh semantic facing, silhouette, material appearance, topology suitability and output rights remain human review decisions; an axis convention alone cannot prove that an AI-generated cart faces the requested direction.

Promotion requires the exact receipt hash, matching capture, matching profile stable ID, a named reviewer and an explicit rights statement. It uses the existing atomic, create-only bundle promotion and production-reference guard. No live asset or production definition is changed by automated tests.

## Verification

```powershell
Scripts/RunGenerationWorkerTests.ps1
Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Architecture.StagedMedia -WithRendering
Scripts/VerifyShippingExclusion.ps1
Scripts/VerifyMediaShipping.ps1
```

CI runs the worker mocks and a dedicated rendering proof; it never enables or calls a live Tripo account. The Tripo tests use synthetic responses shaped from documentation and original mathematical meshes, not recordings claimed to come from a paid generation. Native tests cover import normalization, resource/collision/axis failures, capture requirements, stable-ID matching and approved promotion. Shipping scans include Tripo adapter/API/credential/configuration tokens.

The full MVP manual live-generation demo remains a separate explicitly budgeted acceptance run. No live spend was performed for S13-P02. The previously documented Landmass cook blocker is not resolved by this change; binary exclusion and production-reference audits are not a substitute for a passing full cook.

## API references

Contract checked 2026-09-06 against official [text generation](https://developers.tripo3d.ai/en/docs/generation-text-to-model/standard), [image generation](https://developers.tripo3d.ai/en/docs/generation-image-to-model/standard), [file upload](https://developers.tripo3d.ai/en/docs/files), [task query](https://developers.tripo3d.ai/en/docs/task-query) and [task lifecycle](https://developers.tripo3d.ai/en/docs/task-lifecycle) documentation. The native importer axis mapping was also checked against UE 5.8 `GLTF/ConversionUtilities.h`.
