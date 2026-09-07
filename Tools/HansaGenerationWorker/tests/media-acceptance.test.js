import assert from "node:assert/strict";
import { test } from "node:test";
import { mkdtemp, readFile, writeFile, rm, access, mkdir } from "node:fs/promises";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { fileURLToPath } from "node:url";
import os from "node:os";
import path from "node:path";
import { sha256 } from "../src/canonical-json.js";

test("media acceptance survives process kill, selects audio variant two, and cleans only its own fixtures", async t => {
  const project = await mkdtemp(path.join(os.tmpdir(), "hansa-media-acceptance-"));
  t.after(() => rm(project, { recursive: true, force: true }));
  const evidence = path.join(project, "Saved/evidence");
  const script = fileURLToPath(new URL("../scripts/media-acceptance.js", import.meta.url));
  // Ambient live enable flags must never select real transports in this harness.
  const run = phase => promisify(execFile)(process.execPath, [script, phase, project, evidence], {
    timeout: 90000, env: { ...process.env, HANSA_TRIPO_ENABLED: "1", HANSA_ELEVENLABS_ENABLED: "1",
      TRIPO_API_KEY: "ambient-secret-must-not-be-used", ELEVENLABS_API_KEY: "ambient-secret-must-not-be-used" }
  });
  await run("prepare");
  const planPath = path.join(evidence, "media-plan.json");
  const plan = JSON.parse(await readFile(planPath, "utf8"));
  const report = JSON.parse(await readFile(path.join(evidence, "worker-acceptance.json"), "utf8"));
  assert.equal(report.status, "Succeeded"); assert.equal(report.liveProviderCalls, false);
  assert.equal(report.resumedWithoutPropResubmission, true);
  assert.deepEqual(plan.items.map(item => item.revision), [1, 2, 1]);
  assert.deepEqual(plan.items.map(item => item.outputIndex), [0, 1, 1]);
  await assert.rejects(run("prepare"), /EEXIST/);
  const bad = structuredClone(plan); bad.jobsRelative = "Saved";
  await writeFile(planPath, JSON.stringify(bad));
  await assert.rejects(run("cleanup"));
  await access(path.join(project, plan.jobsRelative));
  await writeFile(planPath, JSON.stringify(plan));
  await run("discard-jobs");
  await assert.rejects(access(path.join(project, plan.jobsRelative)));
  for (const item of plan.items) {
    const source = JSON.parse(await readFile(path.join(project, item.descriptorPath), "utf8"));
    assert.equal(sha256(await readFile(path.join(project, source.sourcePath))), item.sourceSha256);
    const manifest = await readFile(path.join(project, source.manifestPath), "utf8");
    assert.ok(!manifest.includes("ambient-secret"));
  }
  // Unreal uses digits-only GUIDs for staging folders, unlike worker UUID paths.
  const stage = path.join(project, "Content/Hansa/Generated/Staging", plan.items[0].jobId.replaceAll("-", ""));
  await mkdir(stage, { recursive: true });
  await writeFile(path.join(stage, "fixture.uasset"), "test-only staging sentinel");
  await run("cleanup");
  await assert.rejects(access(stage));
  for (const item of plan.items) await assert.rejects(access(path.join(project, item.descriptorPath)));
  await access(planPath); // Evidence survives fixture cleanup.
});
