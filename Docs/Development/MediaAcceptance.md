# S13-P04 media acceptance and recovery

This is the connected, mock-first proof for one Tripo harbor prop, one ElevenLabs SFX and one English speech take. It exercises the real provider adapters with injected synthetic HTTP responses, the real bounded MP3 decoder and the same native staging/promotion services used by the Editor console. It never invokes environment-selected live providers. Ambient credentials and enable flags cannot turn this runner into a billable workflow.

## Automated acceptance

From a checkout with the supported Unreal toolchain, Node.js and FFmpeg on PATH:

```powershell
Scripts/RunMediaAcceptance.ps1
```

Use `-SkipBuild` only after building the current Editor sources. The runner is included in `Scripts/InvokeCI.ps1`. The ordinary worker test suite also exercises the process harness and cleanup guards; no provider account is needed. FFmpeg must support MP3 decoding. Missing tooling fails with evidence rather than silently substituting a fake decoder.

The connected sequence is:

1. Submit the prop, SFX and speech requests and observe durable job progress.
2. Cancel the queued SFX and explicitly approve its second mock revision.
3. Start the prop and kill a real worker process once its provider task ID and Running state have been persisted.
4. Start a fresh worker, resume all three jobs and assert the prop is not submitted twice. Generate two takes for each audio role and retain take two.
5. Verify source/manifest hashes and retain real worker files under `SourceArt/Generated`. Preserve original MP3s, model/settings, simulated usage and explicit mathematical-fixture provenance.
6. Start Unreal, stage those exact retained outputs and validate the native mesh/audio contracts. Render the existing 1280×720 prop scene. Assert an unapproved promotion is rejected.
7. Inject a failure before committing the prop and prove no partial final directory exists. Retry through the ordinary approval service; promote all three isolated fixture bundles.
8. Archive then actually delete this run's transient `Saved/GenerationJobs/MediaAcceptance_<runId>` directory.
9. Start a second Unreal process. Load final packages and validate stable identities, subtitles, sound settings, geometry and the retained promotion chain without worker state.
10. Copy retained provenance into the evidence bundle and remove this run's source/staging/final fixture directories. Evidence remains in Saved/BuildArtifacts.

Native automation uses explicit **test-only approval and audio-playback markers** because it is headless and does not represent a human listener. These markers are written only by the automation test for manifests explicitly marked as mathematical mocks; no production preview/approval bypass API was added. The mesh capture is rendered, but a successful run does not certify human visual review, intelligible speech or artistic suitability. The speech fixture is a sine tone carrying test subtitle metadata.

The aggregate `result.json` reports worker, native promotion and fresh-process verification results. The bundle includes `media-plan.json`, `worker-acceptance.json`, `worker-review/<role>/events.ndjson`, archived pre-deletion job state, `editor-promote.json`, `editor-verify.json`, `retained-<role>/` source/manifest/receipt/approval files, a native prop capture and Unreal logs. Copied records preserve their original project paths; after test cleanup they are evidence snapshots, not live production assets.

## Clean manual mock demo

Build/open the current Editor normally with audio enabled; do not use the headless automation runner for human review. Use a fresh evidence directory for each demo:

```powershell
node Tools/HansaGenerationWorker/scripts/media-acceptance.js prepare . Saved/BuildArtifacts/MediaManualDemo01
$demo = Get-Content Saved/BuildArtifacts/MediaManualDemo01/media-plan.json -Raw | ConvertFrom-Json
$demo.items | Format-Table kind, jobId, revision, outputIndex, stableId
$demo.items | ForEach-Object { 'Hansa.Media.Stage "' + $_.descriptorPath + '"' }
```

The prepare command prints Queued, cancellation/retry, durable Running/restart and Review milestones. Inspect `worker-review/<role>/events.ndjson` for correlated progress, and the retained `job-manifest.json` for provider/model/settings, rights and hashes. The SFX is revision two, and audio `outputIndex=1` means the selected second take. This mock harness is isolated from the normal worker queue/Studio connection.

For each of the three generated Stage commands, run it in Unreal's Output Log console. Copy its returned receipt path, then:

```text
Hansa.Media.Preview "<receipt printed by Stage>"
```

Inspect the prop in the native asset editor and retained scene capture. Play the SFX and speech test tones in the native Wave editor; check non-looping playback, channel/rate/duration and the speech subtitle `Welcome to the harbor.`. The tone proves playback and metadata plumbing, not an actual merchant performance. Reject any import or hash discrepancy. Spend approval alone must not promote anything.

After reviewing a fixture, copy its exact `destination` and `stableId` from `media-plan.json` and the review hash printed by Preview. A named human reviewer explicitly approves the mathematical test fixture:

```text
Hansa.Media.Promote "<receipt>" "<destination from plan>" "<stableId from plan>" "<reviewer>" "Original mathematical demo fixture reviewed; test use only" "<review hash>" APPROVE
Hansa.Media.Verify "<receipt>"
```

Record each receipt, destination, stable ID, review hash, reviewer, observed result and any rejection in a `manual-review.md` beside the plan. The saved approval journal is the durable approval record; never edit it or manufacture a preview marker. A successful manual report explicitly says all three roles were inspected/listened to and approved, or identifies the incomplete role.

After all three promotions, close the Editor, delete only this demo's transient jobs using the bounded helper, reopen the Editor and run Verify for every saved receipt:

```powershell
node Tools/HansaGenerationWorker/scripts/media-acceptance.js discard-jobs . Saved/BuildArtifacts/MediaManualDemo01
```

Use each promotion journal's `assets[].objectPath` to load the final native assets. Confirm they live under the planned final destination and no definition points into staging. This demo does not silently retarget existing game definitions. Run the production-reference audit separately:

```powershell
Scripts/VerifyMediaShipping.ps1
```

Keep the source and Content records together if preserving the approved demo. For a disposable demo, close its asset editors and use the exact run-bound cleanup command after saving any desired evidence:

```powershell
node Tools/HansaGenerationWorker/scripts/media-acceptance.js cleanup . Saved/BuildArtifacts/MediaManualDemo01
```

Cleanup rejects changed run paths, invalid IDs and symlink/reparse traversal. It deletes only the plan's generated fixture directories, not all Saved state or unrelated assets. Never reuse a demo directory or a production destination.

## Optional live smoke

Live smoke is separate from normal acceptance and is never required for CI success. Obtain explicit authorization for the exact provider, prompt/reference upload, one or two variants, approved voice/rights, current rate card and per-job/total budget before enabling a dedicated low-privilege worker credential. Follow [TripoStaticProps.md](TripoStaticProps.md) or [ElevenLabsAudioTakes.md](ElevenLabsAudioTakes.md) for opt-in environment configuration and disabled request templates.

Use the provider-specific CLI `estimate` then `submit --approve-spend=<reviewer> --confirm-cost=<exact-estimate>`. Never place credentials in arguments, requests, screenshots or evidence. Observe `status`, retain the selected output, and use the same Stage → Preview → explicit Promote → Verify sequence above with a new production destination. Record actual usage and source/output hashes. For speech, confirm the heard words match the subtitle and approved English speaker role. For SFX, reject unintended speech; for the prop, inspect silhouette, scale, facing, materials and collision.

A local cancellation may still incur provider charges. An unknown submission outcome requires checking the provider account before approving any retry. ElevenLabs missing reported usage fails closed. Live voice cloning, multiple speakers and localization batches remain excluded.

Archive the selected source and its provenance before deleting transient jobs. Reopen Unreal and verify final packages again. Any live run needs its own dated evidence and human approval; the mathematical mock evidence must never be presented as a real generated asset or voice performance.

## Scope and release gates

No gameplay schema or runtime dependency changes are introduced by S13-P04. Existing canonical profile versions, migration behavior, stable IDs and create-only promotion semantics remain authoritative. Components reused: native Output Log console, mesh/Wave editors, and the existing neutral prop scene/capture. No new UI design, raster production asset, generation mode or prompt set is introduced.

The runner proves recovery and provenance, not a complete Shipping cook or the integrated game demo. Run Shipping exclusion and cooked-content gates separately. Existing serialized Windmill transient paths and the Landmass cook blocker remain independent issues. See [S13-P04 evidence](Evidence/S13P04-20260906.md) for measured results and limits.
