# ElevenLabs SFX and speech takes (S13-P03)

The external worker supports one short non-looping harbor/UI SFX and one short, single-speaker English line, with one or two variants per request. Native staging validates and previews a selected take; promotion requires explicit human approval.

## Scope and contracts

- Provider `elevenlabs`, adapter `1.0.0`.
- `TextToSoundEffect` / `HarborSFX` uses model ID `eleven_text_to_sound_v2`.
- `TextToSpeech` / `SpeechLine` uses model ID `eleven_flash_v2_5`, one configured approved voice and `language_code=en`.
- [AudioTake v1 schema](../../Tools/HansaGenerationWorker/schemas/audio-take-v1.schema.json) defines stable SFX/Dialogue identity, variant count, duration and silence limits, subtitle, speaker identity, language and non-looping policy.
- No voice cloning endpoints, multi-speaker generation, pronunciation dictionaries, dialogue graphs, localization batches, looping ambience or automated loudness mastering.
- Model identifiers are explicitly selected; the provider does not expose an immutable backend revision. Record output hashes rather than claiming reproducible generation from a seed.

The profile is additive to retained-source v1. Existing generic WAV and static-prop sources remain compatible. ElevenLabs sources must have an audio profile. Profile changes require a new reviewed job/revision and staging receipt; incompatible semantics need a new profile version.

## Worker configuration

Use a dedicated, appropriately restricted account credential. Keep all variables below in the external worker process environment or approved credential launcher, never Unreal project/runtime configuration.

| Variable | Required configuration |
| --- | --- |
| `HANSA_ELEVENLABS_ENABLED=1` | Explicit opt-in; otherwise adapter is absent |
| `ELEVENLABS_API_KEY` | Worker-only credential |
| `HANSA_ELEVENLABS_APPROVED_VOICES` | Comma-separated approved stock/library voice IDs; empty disables advertised speech capability |
| `HANSA_ELEVENLABS_SFX_UNITS_PER_SECOND` | Conservative integer estimate of billing-character units per requested SFX second |
| `HANSA_ELEVENLABS_SPEECH_UNITS_PER_CHARACTER` | Conservative integer estimated billing units per source character |
| `HANSA_ELEVENLABS_MINOR_UNITS_PER_1000` | Currency minor units per 1000 billed units |
| `HANSA_ELEVENLABS_CURRENCY` | Currency code, default USD |
| `HANSA_AUDIO_DECODER` | FFmpeg executable path or command; default ffmpeg |

Configure rates from the current account/voice/model terms, including applicable multipliers. No price is hardcoded. The worker estimates the total for all requested takes and checks the request's explicit ceiling. It also checks observed `character-cost` response headers and remaining budget before another take. Missing/invalid reported usage fails closed; inspect account billing before retrying. Local cost admission cannot impose a remote provider-account cap.

FFmpeg must be installed on development/CI hosts. Its availability is checked before the first billable request; its version is recorded in successful provenance. The decoder runs without a shell, accepts only an in-memory MP3 pipe, denies file/network input protocols, and has time/output/diagnostic limits. The tested host uses FFmpeg 8.0.1. No binary is bundled into the game or Shipping package.

Start the authenticated worker using [GenerationWorker.md](GenerationWorker.md). Copy either the [SFX template](../../Tools/HansaGenerationWorker/examples/elevenlabs-sfx.json) or [speech template](../../Tools/HansaGenerationWorker/examples/elevenlabs-speech.json). Templates default to zero spend and unacknowledged rights. Set a unique action key, actual rights statement, limits, approved cost ceiling and, for speech, the authorized voice ID.

```powershell
node Tools/HansaGenerationWorker/scripts/elevenlabs.js estimate <request.json>
node Tools/HansaGenerationWorker/scripts/elevenlabs.js submit <request.json> --approve-spend=<reviewer> --confirm-cost=<total-estimated-minor-units>
node Tools/HansaGenerationWorker/scripts/elevenlabs.js status <job-id>
node Tools/HansaGenerationWorker/scripts/elevenlabs.js cancel <job-id>
```

Speech additionally requires `rights.voiceAcknowledged=true`, `rights.englishTextAcknowledged=true`, exact equality of prompt and subtitle, a stable `Speaker.*` identity and a voice from the configured allowlist. Use an authorized existing stock/library voice; this workflow does not create voices. English acknowledgement and model language selection do not prove that a generated waveform says the correct words: listen before promotion.

The CLI re-estimates and requires an exact confirmed total plus a named approver. Spend approval and content-promotion approval remain separate. Request parameters cannot contain credentials, private/redacted prompt placeholders, unsupported settings or more than two takes.

## Technical audio gate

The API returns requested MP3 audio at 44.1 kHz/128 kbps. The worker preserves the original bytes, decodes to PCM16 WAV and verifies the actual 44.1 kHz sample rate. It does not resample, downmix, trim, repair clipping or master loudness.

Canonical audio QA checks:

- Decodable bounded RIFF/WAV PCM16, mono/stereo SFX, mono speech.
- Allowed canonical source rates: 24, 44.1 or 48 kHz; this adapter specifically requests/verifies 44.1 kHz.
- Duration inside the approved interval, with an absolute 15-second ceiling. SFX requested generation duration is 0.5–10 seconds.
- No samples with absolute PCM value 32760 or greater.
- Activity threshold: absolute PCM value greater than 104, approximately -50 dBFS.
- At least one active frame; leading silence at most the approved threshold (maximum 500 ms), trailing silence at most its approved threshold (maximum 1000 ms).
- Encoded source at most 2 MiB per take, decoded WAV at most 4 MiB per take and at most 8 MiB combined/request, also subject to the lower user byte budget.

The same duration/channel/rate/clipping/silence checks run over Unreal's decoded SoundWave source. Native staging sets non-looping playback, PCM compression, Effects or Voice sound group, and a single time-zero subtitle for speech. Subtitle text and the stable `Hansa.Dialogue` text key must still match at promotion.

These are technical checks, not a semantic or artistic score. Wrong words, poor pronunciation, unsuitable voice, audible artifacts below clipping thresholds or an inappropriate bell still require rejection by the reviewer.

## Retain, compare and promote

After Review:

```powershell
node Tools/HansaGenerationWorker/scripts/elevenlabs.js retain <job-id> --take=1
node Tools/HansaGenerationWorker/scripts/elevenlabs.js retain <job-id> --take=2
```

Select only take 1 when the request contains one variant. Each retained directory contains immutable canonical `source.wav`, original `provider-original.mp3`, completed `job-manifest.json` and `source.json`. Both source hashes are verified. Successful provenance records each take's provider request ID when supplied, actual settings/voice/model, billing units, technical QA, original/output hashes and decoder version. Provider request/voice IDs never become gameplay identity.

Stage each candidate into its own isolated receipt, compare in the existing native SoundWave editors, then preview and approve the selected take:

```text
Hansa.Media.Stage <SourceArt/Generated/.../source.json>
Hansa.Media.Preview <SourceArt/Generated/.../import-....json>
Hansa.Media.Promote <receipt> </Game/Hansa/Audio/NewFeature> <SFX.HarborBell-or-Dialogue.DockGreeting> <reviewer> "<output rights statement>" <review-hash> APPROVE
```

Preview opens the native editor and starts playback. An unavailable audio device prevents the playback-review marker. The marker means playback started, not that a human approved it; listen to the whole selected take and check the subtitle before issuing APPROVE. There is no new GUI design.

Promotion checks the exact receipt, source hashes, stable ID, playback marker, native audio QA, named reviewer, rights statement, and new explicit destination. Final assets use `SW_SFX_...` or `SW_Dialogue_...` names and stable metadata. Speech subtitles remain native SoundWave content with stable text keys. Atomic create-only promotion, source-data rebasing and reference auditing are shared with [StagedMedia.md](StagedMedia.md). Durable provenance does not depend on Saved job state.

## Cancellation, retry and restart

These generation endpoints return audio synchronously; the worker wraps them in its asynchronous job lifecycle. It never invents a provider polling or cancellation endpoint. Local cancellation aborts transport/decoding, but processing and billing may already have occurred.

Billable POSTs are never automatically retried. The durable submission-intent guard prevents replay after an ambiguous crash. Completed persisted batch state resumes without a new provider call. An incomplete/ambiguous batch fails and requires account inspection and a newly approved retry. A second-take failure never presents a partial batch as Review; first-take source/hash and known billing are preserved in failure evidence. Known costs are also preserved when cancellation interrupts a later take.

```powershell
node Tools/HansaGenerationWorker/scripts/elevenlabs.js retry <job-id> --approve-spend=<reviewer> --confirm-budget=<original-ceiling> --retry-key=<unique-action-key>
```

A retry is another explicitly budgeted revision, not a silent free regeneration. Model sampling remains nondeterministic.

## Validation and live acceptance

Normal CI runs mocks and original mathematical audio fixtures. It also exercises the real bounded FFmpeg decoder and native import/promotion. No API credentials or live billing are required. See [S13-P03 evidence](Evidence/S13P03-20260906.md).

A live smoke run requires separate explicit spend authorization, configured rates/voice rights and human listening. Neither synthetic tones nor mock provider responses prove a real generated SFX or spoken line. The existing Windmill serialized-path convention failures and Landmass full-cook blocker remain independent acceptance limitations.

API contracts checked 2026-09-06 against official [SFX generation](https://elevenlabs.io/docs/api-reference/text-to-sound-effects/convert) and [speech generation](https://elevenlabs.io/docs/api-reference/text-to-speech/convert) documentation.
