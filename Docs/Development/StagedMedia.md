# S13-P01 shared staged media

The provider-neutral worker retains validated sources; HansaEditor imports, previews, approves and promotes native assets. There are no provider SDKs, credentials, network downloads or provider-dependent gameplay IDs in runtime modules. No live generation or production media promotion was performed while implementing this task.

## Contracts and retained records

Media contract v1 accepts self-contained static GLB 2 and 16-bit PCM WAV (mono/stereo; 22050, 24000, 44100 or 48000 Hz), up to 64 MiB per source. It rejects external URIs, GLB extensions, rigs/animations, malformed container lengths, unsupported codecs and changed hashes. Detailed asset-role budgets, collision/pivot QA, audio clipping/silence QA and provider adapters belong to S13-P02/P03. FBX, MP3, skeletal assets and external texture bundles are not admitted by v1.

`job.media.retain` is advertised only when the worker has a configured project root. Its authenticated payload is `{jobId, outputIndex}`; it does not accept an arbitrary destination. It rechecks the completed Review manifest and output bytes, then writes:

- `SourceArt/Generated/<Meshes|Audio>/<JobId>/r<Revision>/artifact-<N>/source.glb|wav`
- `job-manifest.json`: immutable original request/provenance, provider/model/adapter, prompt, input rights/hashes, usage, settings and QA.
- `source.json`: versioned descriptor, source SHA-256, manifest document hash and exact manifest-file SHA-256.

The descriptor schema is `Tools/HansaGenerationWorker/schemas/media-source-v1.schema.json`. Revision-one Saved output paths remain compatible. Later retry downloads use `output/r<Revision>/` so new bytes cannot replace an earlier attempt. Unsupported descriptor/receipt versions fail closed and require a new import from retained source; existing records are never migrated in place.

The editor adds `import-<BundleId>.json` beside the source. It records the source-descriptor hash and every native staged package hash. Each import gets its own directory below `/Game/Hansa/Generated/Staging/<JobId>/<BundleId>`. No importer writes over a production asset.

## Review and promotion

Start the worker using the existing [worker setup](GenerationWorker.md), then retain one completed media output:

```powershell
node Tools/HansaGenerationWorker/scripts/retain-media.js <Hansa-job-UUID> 0
```

Use the existing Unreal Editor Output Log console:

```text
Hansa.Media.Stage "SourceArt/Generated/Audio/<JobId>/r1/artifact-1/source.json"
Hansa.Media.Preview "<project-relative import receipt printed by Stage>"
Hansa.Media.Promote "<receipt>" "/Game/Hansa/Audio/HarborBellV1" "SFX.HarborBell" "Reviewer name" "Commercial use approved; terms version and restrictions reviewed" "<hash printed by Preview>" APPROVE
Hansa.Media.Verify "<receipt>"
```

Preview opens Unreal's native asset editors for all bundle assets. Inspect the actual mesh/materials or listen to the audio before approving. Spend permission does not authorize promotion. Promotion requires a named reviewer, a nonempty output-rights statement, an explicit new destination, a Hansa `Prop.*`, `SFX.*` or `Dialogue.*` ID, and the exact preview hash.

The service reruns native data validation, geometry/audio sanity checks, source/manifest/package hashes, dirty-package checks and production referencer checks. It duplicates all bundle assets under Hansa-owned names, remaps internal hard and soft references, checks the resulting object graph for staging/developer dependencies and writes all final packages into a temporary directory. Asset metadata stores the stable ID, retained receipt, source/manifest/review hashes, reviewer and rights decision. Importer source filenames are rebased to the retained source for the final package location. Full provenance is reached through the retained receipt/descriptor/manifest chain.

The entire destination directory appears through one same-volume directory rename. An OS-held per-receipt lock serializes competing Editor processes and releases on a crash. Existing directories or assets cannot be replaced. A replacement must use a new feature/revision directory and the existing authoring reference picker; this service intentionally has no in-place overwrite operation. Internal bundle references change atomically with its assets. Existing gameplay definition references are not implicitly retargeted.

Include the new Content packages and their SourceArt records in the same source-control change. Native editor transactions cover creation during the operation; after persistence, rollback is an explicit reviewed content revision, not a promise that Undo reverses committed filesystem writes.

## Recovery

An immutable approval journal (`.promotion.json`) is saved beside the import receipt before commit. Failure before the directory move leaves no production bundle. Normal failure discards temporary objects/files and removes the uncommitted decision; an interrupted process leaves evidence which a matching retry archives before trying again.

If the process stops after commit, retrying the same receipt/destination/ID verifies all final hashes rather than overwriting. `Hansa.Media.Verify` works from SourceArt and production package files without Saved job state or staging packages. It recreates a missing `.committed.json` terminal marker only after verifying the source chain and final package hashes. Changed production bytes fail verification and are preserved.

## Validation and release gate

```powershell
pwsh -NoProfile -File Scripts/RunGenerationWorkerTests.ps1
pwsh -NoProfile -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Architecture.StagedMedia
pwsh -NoProfile -File Scripts/VerifyShippingExclusion.ps1
pwsh -NoProfile -File Scripts/RunMediaCookAudit.ps1
```

The deterministic golden fixtures `Tests/Golden/Media/triangle.glb` and `tone.wav` are original mathematical fixtures, not production media. Worker tests cover immutable retention, URI/format rejection, rights, tampering, authentication, retries and deletion of Saved state. Native tests cover real mesh/audio import, approval/preview gating, injected failure/retry, idempotent commit recovery, final tamper detection and an actual production redirector referencing staging.

`HansaMediaAudit` scans all /Game Asset Registry package dependencies, including hard and soft references. `VerifyMediaShipping.ps1 -CookedRoot <expanded-cooked-tree> -RequireCookedContent` requires actual cooked packages, rejects opaque containers, scans paths and serialized package/registry/config tokens, and records hashes. The existing never-cook rules for Hansa staging/developer content remain active. `InvokeCI.ps1` now runs this gate; it cooks by default or accepts an explicit `-CookedRoot`.

The isolated media cook disables ModelContextProtocol, AllToolsets, GameFeatures, Water and Landmass only for the test process. The unmodified project cook currently fails on a busy MCP HTTP port, missing GameFeatureData asset-manager settings and Landmass startup assets referencing engine editor materials. The override is recorded in evidence and does not alter the project configuration. The isolated retry still fails on Landmass startup references. Therefore no successful cooked-content or full-package proof is claimed. The cook gate remains blocking; the separate production-referencer and Shipping executable audits pass.

No new GUI artwork or shipping raster assets are introduced. Stage, Preview, Promote and Verify are shared editor services exposed through the existing console and native asset editors; provider-specific Authoring Studio flows can call the same service in S13-P02/P03/P04.


## S13-P02 static harbor props

The Tripo v3 adapter, canonical HarborProp profile, native normalization/collision QA, deterministic capture and explicit spend/promotion workflow are documented in [TripoStaticProps.md](TripoStaticProps.md). Generic retained media keeps the existing v1 contract; Tripo adds a required versioned static-prop profile.

## S13-P03 audio takes

The ElevenLabs adapters, versioned audio profile, decode/PCM QA, stable subtitles, native playback and approval workflow are documented in [ElevenLabsAudioTakes.md](ElevenLabsAudioTakes.md). [S13-P03 evidence](Evidence/S13P03-20260906.md) records automated results and remaining manual/release gates.

## S13-P04 connected acceptance

[MediaAcceptance.md](MediaAcceptance.md) documents the mock-only real-process recovery runner, native promotion/reload proof, clean manual demo and optional budgeted live smoke. Run `Scripts/RunMediaAcceptance.ps1` for aggregate evidence.
