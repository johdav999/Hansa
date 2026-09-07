import assert from "node:assert/strict";
import { readFile, writeFile, mkdir, rm, cp, lstat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { fork } from "node:child_process";
import { randomUUID } from "node:crypto";
import { JobStore } from "../src/job-store.js";
import { GenerationWorkerService } from "../src/worker-service.js";
import { TripoStaticPropProvider } from "../src/providers/tripo-static-prop-provider.js";
import { ElevenLabsAudioProvider } from "../src/providers/elevenlabs-audio-provider.js";
import { sha256, hashDocument } from "../src/canonical-json.js";

// No environment provider factory or real fetch is used. Unknown mock routes throw.
const [phase, projectArg, evidenceArg] = process.argv.slice(2);
assert.ok(["prepare", "queue", "resume", "discard-jobs", "cleanup"].includes(phase) && projectArg && evidenceArg,
  "Usage: media-acceptance.js prepare|discard-jobs|cleanup <project-root> <new-evidence-root>");
const project = path.resolve(projectArg), evidence = path.resolve(evidenceArg);
const planFile = path.join(evidence, "media-plan.json");
const save = (file, value) => writeFile(file, JSON.stringify(value, null, 2));
const read = async file => JSON.parse(await readFile(file, "utf8"));
const approval = () => ({ approved: true, approvedBy: "S13-P04 mock automation; no real spend", approvedAt: new Date().toISOString() });
const fixtures = new URL("../../../Tests/Golden/Media/", import.meta.url);

async function child(mode, checkpoint = false) {
  return new Promise((resolve, reject) => {
    const processChild = fork(fileURLToPath(import.meta.url), [mode, project, evidence], { windowsHide: true, stdio: ["ignore", "inherit", "inherit", "ipc"] });
    let reached = false;
    const timer = setTimeout(() => { processChild.kill(); reject(new Error("Mock media child timed out")); }, 60000);
    processChild.on("error", reject);
    processChild.on("message", message => {
      if (checkpoint && message === "persisted-running") { reached = true; processChild.kill(); }
    });
    processChild.on("exit", code => {
      clearTimeout(timer);
      if (checkpoint ? reached : code === 0) resolve(); else reject(new Error(`Mock ${mode} failed: ${code}`));
    });
  });
}
async function checkedPlan() {
  const plan = await read(planFile);
  assert.equal(plan.version, 1); assert.equal(plan.mockOnly, true);
  assert.match(plan.runId, /^[0-9a-f]{32}$/);
  assert.equal(path.resolve(plan.projectRoot), project);
  assert.equal(plan.jobsRelative, `Saved/GenerationJobs/MediaAcceptance_${plan.runId}`);
  assert.equal(plan.items.length, 3);
  for (const item of plan.items) {
    assert.ok(["Prop", "SFX", "Speech"].includes(item.kind));
    assert.match(item.jobId, /^[0-9a-f-]{36}$/);
    const domain = item.kind === "Prop" ? "Meshes" : "Audio";
    assert.equal(item.destination, `/Game/Hansa/${domain}/MediaAcceptance_${plan.runId}_${item.kind}`);
    if (item.descriptorPath) assert.match(item.descriptorPath.replaceAll("\\", "/"), new RegExp(`^SourceArt/Generated/${domain}/${item.jobId}/r[12]/artifact-[12]/source\\.json$`));
  }
  return plan;
}
// Only remove exact fresh-run directories, with no symlink/reparse ancestor traversal.
async function removeScoped(relative) {
  const target = path.resolve(project, relative);
  assert.ok(target.startsWith(project + path.sep) && target !== project);
  let current = project;
  for (const segment of path.relative(project, target).split(path.sep)) {
    current = path.join(current, segment);
    try { assert.equal((await lstat(current)).isSymbolicLink(), false, "Cleanup cannot traverse links"); }
    catch (error) { if (error.code === "ENOENT") return; throw error; }
  }
  console.log(`Removing isolated fixture directory: ${target}`);
  await rm(target, { recursive: true, force: true });
}
if (phase === "prepare") {
  await mkdir(evidence, { recursive: true });
  const runId = randomUUID().replaceAll("-", "");
  await writeFile(planFile, JSON.stringify({ version: 1, mockOnly: true, runId, projectRoot: project,
    jobsRelative: `Saved/GenerationJobs/MediaAcceptance_${runId}`, items: [] }, null, 2), { flag: "wx" });
  await child("queue", true); // Kill a real process after its provider ID and Running state are durable.
  await child("resume");
  console.log(`Mock sources ready for native review. Plan: ${planFile}`);
} else if (phase === "discard-jobs" || phase === "cleanup") {
  const plan = await checkedPlan();
  if (phase === "discard-jobs") {
    await cp(path.join(project, plan.jobsRelative), path.join(evidence, "worker-state-before-deletion"), { recursive: true, errorOnExist: true, force: false });
  }
  await removeScoped(plan.jobsRelative);
  if (phase === "cleanup") for (const item of plan.items) {
    const domain = item.kind === "Prop" ? "Meshes" : "Audio";
    await removeScoped(`SourceArt/Generated/${domain}/${item.jobId}`);
    await removeScoped(`Content/Hansa/Generated/Staging/${item.jobId.replaceAll("-", "")}`);
    await removeScoped(item.destination.replace("/Game/", "Content/"));
  }
} else {
  const plan = await read(planFile);
  const glb = await readFile(new URL("harbor-prop.glb", fixtures));
  const mp3 = await readFile(new URL("audio-take.mp3", fixtures));
  let postCount = 0;
  const envelope = data => new Response(JSON.stringify({ code: 0, data }), { headers: { "content-type": "application/json" } });
  const tripo = new TripoStaticPropProvider({ apiKey: "mock-only-not-a-credential", estimatedCredits: 10, minorUnitsPerCredit: 2,
    fetchImpl: async (url, init = {}) => {
      const u = new URL(url);
      if (u.hostname === "openapi.tripo3d.ai" && init.method === "POST") { postCount++; return envelope({ task_id: "mock_media_acceptance" }); }
      if (u.hostname === "openapi.tripo3d.ai" && u.pathname.includes("/tasks/")) {
        if (phase === "queue") {
          // Worker has saved the submission result before entering Poll.
          console.log("Prop: Running with durable task ID; forcing worker restart");
          process.send("persisted-running");
          return new Promise(() => { setInterval(() => {}, 1000); });
        }
        return envelope({ task_id: "mock_media_acceptance", status: "success", credits_consumed: 10,
          output: { model_url: "https://cdn.tripo3d.ai/mock/harbor.glb" } });
      }
      if (u.href === "https://cdn.tripo3d.ai/mock/harbor.glb") return new Response(glb, { headers: { "content-type": "model/gltf-binary" } });
      throw new Error("Unexpected mocked Tripo route");
    } });
  tripo.pollIntervalMs = 1;
  const audio = new ElevenLabsAudioProvider({ apiKey: "mock-only-not-a-credential", allowedVoiceIds: ["MockVoice"],
    sfxUnitsPerSecond: 100, speechUnitsPerCharacter: 1, minorUnitsPer1000: 10,
    fetchImpl: async (url, init) => {
      const u = new URL(url);
      assert.equal(u.hostname, "api.elevenlabs.io"); assert.equal(init.method, "POST");
      assert.ok(["/v1/sound-generation", "/v1/text-to-speech/MockVoice"].includes(u.pathname));
      postCount++;
      return new Response(mp3, { headers: { "content-type": "audio/mpeg", "character-cost": "100", "request-id": `mock_take_${postCount}` } });
    } }); // Uses the real bounded decoder; no fixture decoder substitution.
  for (const provider of [tripo, audio]) {
    const normalize = provider.normalizeMetadata.bind(provider);
    provider.normalizeMetadata = (...args) => ({ ...normalize(...args), deterministicMock: true,
      fixtureOrigin: "Original mathematical harbor mesh and sine tone; not generated speech or live provider media" });
  }
  const store = new JobStore(path.join(project, plan.jobsRelative));
  const worker = new GenerationWorkerService({ store, providers: [tripo, audio], projectRoot: project, autoProcess: false, pollIntervalMs: 1 });
  await worker.initialize();
  try {
    if (phase === "queue") {
      for (const kind of ["Prop", "SFX", "Speech"]) {
        const prop = kind === "Prop", speech = kind === "Speech";
        const stableId = `${prop ? "Prop" : speech ? "Dialogue" : "SFX"}.Acceptance${plan.runId}.${kind}`;
        const audioTake = { version: 1, kind: speech ? "Speech" : "SFX", stableId, variants: 2,
          minimumDurationMs: 100, maximumDurationMs: 2000, maximumLeadingSilenceMs: 100, maximumTrailingSilenceMs: 150,
          subtitle: speech ? "Welcome to the harbor." : "", speakerId: speech ? "Speaker.Dockworker" : "", language: speech ? "en" : "", loop: false };
        const parameters = prop ? { staticProp: { version: 1, role: "HarborProp", stableId, heightCm: 100,
          maximumTriangles: 1000, maximumMaterials: 2, maximumTextureSize: 1024, forwardAxis: "+X", upAxis: "+Z", pivot: "bottom-center", collision: "box" } }
          : speech ? { audioTake, voiceId: "MockVoice", voiceSettings: { stability: 0.5, similarity_boost: 0.7, style: 0 } }
          : { audioTake, durationSeconds: 1, promptInfluence: 0.3 };
        const request = { schemaVersion: 1, idempotencyKey: `${plan.runId}-${kind}`, providerId: prop ? "tripo" : "elevenlabs",
          modelVersion: prop ? "v3.1-20260211" : speech ? "eleven_flash_v2_5" : "eleven_text_to_sound_v2",
          capability: prop ? "TextToMesh" : speech ? "TextToSpeech" : "TextToSoundEffect", intendedAssetRole: prop ? "HarborProp" : speech ? "SpeechLine" : "HarborSFX",
          prompt: speech ? audioTake.subtitle : "Original mock harbor fixture; no live generation.", parameters, inputArtifacts: [],
          rights: { acknowledged: true, declaration: "Original mathematical fixtures; test-only rights; no real voice", voiceAcknowledged: true, englishTextAcknowledged: true },
          budgets: { currency: "USD", maximumCostMinorUnits: 100, maximumOutputBytes: 1048576 },
          outputContract: { version: 1, maximumArtifacts: prop ? 1 : 2, allowedMediaTypes: [prop ? "model/gltf-binary" : "audio/wav"] },
          timeoutMs: 60000, spendApproval: approval() };
        const { job } = await worker.submit(request);
        assert.equal(job.status, "Queued");
        console.log(`${kind}: Queued (${job.jobId})`);
        plan.items.push({ kind, jobId: job.jobId, stableId, destination: `/Game/Hansa/${prop ? "Meshes" : "Audio"}/MediaAcceptance_${plan.runId}_${kind}` });
      }
      const sfx = plan.items[1];
      await worker.cancel(sfx.jobId); await worker.schedule(sfx.jobId);
      assert.equal((await worker.get(sfx.jobId)).status, "Cancelled");
      console.log("SFX: Cancelled; explicitly approving a new mock retry revision");
      await worker.retry(sfx.jobId, `${plan.runId}-retry`, "Explicit mock cancellation recovery", approval());
      assert.equal((await worker.get(sfx.jobId)).revision, 2);
      await save(planFile, plan);
      await worker.schedule(plan.items[0].jobId);
      throw new Error("Queue process should have been killed at the durable checkpoint");
    }
    await checkedPlan();
    const before = await worker.get(plan.items[0].jobId);
    assert.equal(before.status, "Running"); assert.equal(before.providerJobId, "mock_media_acceptance");
    for (const item of plan.items) {
      await worker.schedule(item.jobId);
      const job = await worker.get(item.jobId);
      assert.equal(job.status, "Review", JSON.stringify(job.errors)); assert.ok(job.resumeCount > 0);
      const events = (await readFile(path.join(store.jobDirectory(item.jobId), "events.ndjson"), "utf8")).trim().split("\n").map(JSON.parse);
      for (const status of ["Draft", "Estimated", "ApprovedToSpend", "Queued", "Running", "Downloading", "ImportedToStaging", "Validating", "Review"])
        assert.ok(events.some(event => event.status === status), `Missing ${item.kind} progress state ${status}`);
      assert.ok(events.some(event => event.event === "job.recovered"));
      if (item.kind === "SFX") assert.ok(events.some(event => event.status === "Cancelled"));
      console.log(`${item.kind}: recovered -> Downloading -> Validating -> Review (revision ${job.revision})`);
      const outputIndex = item.kind === "Prop" ? 0 : 1;
      const retained = await worker.retainMedia(item.jobId, outputIndex);
      assert.equal(sha256(await readFile(path.join(project, retained.sourcePath))), retained.sha256);
      const { manifestHash, ...body } = await read(path.join(project, retained.manifestPath));
      assert.equal(hashDocument(body), manifestHash); assert.equal(body.provenance.deterministicMock, true);
      Object.assign(item, { descriptorPath: retained.descriptorPath, sourceSha256: retained.sha256, manifestHash,
        revision: job.revision, outputIndex, resumeCount: job.resumeCount });
      await cp(store.jobDirectory(item.jobId), path.join(evidence, "worker-review", item.kind), { recursive: true });
    }
    assert.equal(postCount, 4, "Resume must generate only two SFX and two speech takes, never resubmit the prop");
    await save(planFile, plan);
    await save(path.join(evidence, "worker-acceptance.json"), { status: "Succeeded", mockOnly: true, liveProviderCalls: false,
      actualMoneySpentMinorUnits: 0, simulatedUsageOnly: true, killedAfterDurableRunning: true, resumedWithoutPropResubmission: true,
      sfxCancelledAndRetriedRevision: 2, audioSelectedTake: 2, items: plan.items });
  } finally { await worker.closeForRestart(); }
}
