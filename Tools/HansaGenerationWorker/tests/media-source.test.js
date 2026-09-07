import assert from "node:assert/strict";
import { test } from "node:test";
import { readFile, writeFile, mkdtemp, rm, mkdir } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { randomUUID } from "node:crypto";
import { JobStore } from "../src/job-store.js";
import { GenerationWorkerService } from "../src/worker-service.js";
import { DeterministicMockProvider } from "../src/providers/mock-provider.js";
import { validateMediaBytes, retainMediaSource } from "../src/media-source.js";
import { hashDocument, sha256 } from "../src/canonical-json.js";
import { WorkerRequestHandler } from "../src/request-handler.js";
const fixtures = fileURLToPath(new URL("../../../Tests/Golden/Media/", import.meta.url));
const wav = await readFile(path.join(fixtures, "tone.wav"));
const glb = await readFile(path.join(fixtures, "triangle.glb"));

class GoldenProvider extends DeterministicMockProvider {
  constructor(bytes, mediaType) { super(); this.bytes = bytes; this.mediaType = mediaType; }
  async poll(...args) {
    const result = await super.poll(...args);
    result.result.artifacts = [{ logicalName: "provider-name-is-not-gameplay-identity", mediaType: this.mediaType, contentBase64: this.bytes.toString("base64") }];
    return result;
  }
}
async function setup(t, bytes = wav, mediaType = "audio/wav", acknowledged = true) {
  const root = await mkdtemp(path.join(os.tmpdir(), "hansa-media-"));
  t.after(() => rm(root, { recursive: true, force: true }));
  const store = new JobStore(path.join(root, "Saved/GenerationJobs"));
  const service = new GenerationWorkerService({ store, projectRoot: root, providers: [new GoldenProvider(bytes, mediaType)] });
  await service.initialize();
  const submitted = await service.submit({
    schemaVersion: 1, idempotencyKey: randomUUID(), capability: "StructuredDataDraft", intendedAssetRole: "GoldenMedia",
    providerId: "mock", modelVersion: "mock-v1", prompt: "Deterministic test artifact; no provider call.",
    inputArtifacts: [], rights: { acknowledged, declaration: "Original mathematical fixture." },
    budgets: { maximumCostMinorUnits: 0, currency: "USD", maximumOutputBytes: 65536 },
    outputContract: { version: 1, maximumArtifacts: 1, allowedMediaTypes: [mediaType] },
    parameters: {}, timeoutMs: 5000,
    spendApproval: { approved: true, approvedBy: "Automation", approvedAt: "2026-09-06T00:00:00Z" },
  });
  await service.waitForIdle();
  const job = await store.getJob(submitted.job.jobId);
  assert.equal(job.status, "Review");
  return { root, store, service, job };
}
test("golden self-contained GLB and PCM WAV have deterministic validation", () => {
  assert.deepEqual(validateMediaBytes(wav, "audio/wav"), { format: "pcm16", channels: 1, sampleRate: 24000, durationSeconds: 0.25 });
  assert.deepEqual(validateMediaBytes(glb, "model/gltf-binary"), { format: "glb2", meshCount: 1, embedded: true });
  assert.throws(() => validateMediaBytes(wav, "application/octet-stream"));
  assert.throws(() => validateMediaBytes(wav.subarray(0, 30), "audio/wav"));
  const invalid = Buffer.from(wav); invalid.writeUInt16LE(3, 20);
  assert.throws(() => validateMediaBytes(invalid, "audio/wav"));
  const header = Buffer.from(glb); header.writeUInt32LE(1, 4);
  assert.throws(() => validateMediaBytes(header, "model/gltf-binary"));
});
test("GLB external URIs, extensions, animation and broken buffers cannot reach import", () => {
  const docLength = glb.readUInt32LE(12);
  const original = JSON.parse(glb.toString("utf8", 20, 20 + docLength));
  for (const mutate of [
    d => d.buffers[0].uri = "../external.bin",
    d => d.images = [{ uri: "https://example.invalid/image.png" }],
    d => d.extensionsUsed = ["KHR_draco_mesh_compression"],
    d => d.animations = [{}],
    d => d.buffers[0].byteLength = 999999,
  ]) {
    const doc = structuredClone(original); mutate(doc);
    let json = Buffer.from(JSON.stringify(doc)); json = Buffer.concat([json, Buffer.alloc((4 - json.length % 4) % 4, 32)]);
    const bin = glb.subarray(20 + docLength);
    const head = Buffer.alloc(20); head.write("glTF"); head.writeUInt32LE(2, 4); head.writeUInt32LE(20 + json.length + bin.length, 8); head.writeUInt32LE(json.length, 12); head.writeUInt32LE(0x4e4f534a, 16);
    assert.throws(() => validateMediaBytes(Buffer.concat([head, json, bin]), "model/gltf-binary"));
  }
});
for (const [bytes, mediaType] of [[wav, "audio/wav"], [glb, "model/gltf-binary"]]) {
  test("retained " + mediaType + " is immutable, idempotent and independent of Saved", async t => {
    const h = await setup(t, bytes, mediaType);
    const source = await h.service.retainMedia(h.job.jobId, 0);
    assert.deepEqual(await h.service.retainMedia(h.job.jobId, 0), source);
    assert.equal(sha256(await readFile(path.join(h.root, source.sourcePath))), source.sha256);
    assert.equal(sha256(await readFile(path.join(h.root, source.manifestPath))), source.manifestFileSha256);
    const descriptor = JSON.parse(await readFile(path.join(h.root, source.descriptorPath), "utf8"));
    assert.equal(descriptor.schemaVersion, 1);
    await rm(path.join(h.root, "Saved"), { recursive: true });
    assert.equal(sha256(await readFile(path.join(h.root, source.sourcePath))), source.sha256);
    const { manifestHash, ...body } = JSON.parse(await readFile(path.join(h.root, source.manifestPath), "utf8"));
    assert.equal(hashDocument(body), manifestHash);
    assert.equal(body.provider.modelVersion, "mock-v1");
  });
}
test("rights, source integrity, manifest integrity and format failures block retention", async t => {
  const noRights = await setup(t, wav, "audio/wav", false);
  await assert.rejects(noRights.service.retainMedia(noRights.job.jobId, 0), /rights/);
  const corrupt = await setup(t);
  await writeFile(path.join(corrupt.store.jobDirectory(corrupt.job.jobId), corrupt.job.outputs[0].relativePath), "changed");
  await assert.rejects(corrupt.service.retainMedia(corrupt.job.jobId, 0), /hash/);
  const manifest = await setup(t);
  const file = path.join(manifest.store.jobDirectory(manifest.job.jobId), manifest.job.manifests[0].relativePath);
  const doc = JSON.parse(await readFile(file)); doc.prompt = "tampered"; await writeFile(file, JSON.stringify(doc));
  await assert.rejects(manifest.service.retainMedia(manifest.job.jobId, 0), /integrity/);
  const invalid = await setup(t, Buffer.from("not audio"));
  await assert.rejects(invalid.service.retainMedia(invalid.job.jobId, 0), /WAV/);
});
test("immutable destination cannot be silently overwritten and operation stays authenticated", async t => {
  const h = await setup(t);
  const source = await h.service.retainMedia(h.job.jobId, 0);
  await writeFile(path.join(h.root, source.sourcePath), "manual edit");
  await assert.rejects(h.service.retainMedia(h.job.jobId, 0), /collision/);
  await assert.rejects(retainMediaSource(h.store, undefined, h.job.jobId, 0), /configured/);
  const handler = new WorkerRequestHandler({ service: h.service, authenticationToken: "media-test-token-123456" });
  const reply = await handler.handle({ protocol: "hansa.generation.worker", version: "1.0", requestId: randomUUID(), authToken: "invalid", operation: "job.media.retain", payload: { jobId: h.job.jobId, outputIndex: 0 } });
  assert.equal(reply.error.code, "AuthenticationFailed");
  await assert.rejects(h.service.retainMedia(h.job.jobId, -1));
});

test("retry revisions retain different downloads without overwriting prior bytes", async t => {
  const h = await setup(t);
  const first = h.job.outputs[0];
  const changed = Buffer.from(wav); changed.writeInt16LE(1234, 44);
  const outputs = await h.store.writeArtifacts({ ...h.job, revision: 2 }, [{
    logicalName: "new-variant", mediaType: "audio/wav", contentBase64: changed.toString("base64")
  }]);
  assert.equal(outputs[0].relativePath, "output/r2/artifact-001.wav");
  assert.equal(sha256(await readFile(path.join(h.store.jobDirectory(h.job.jobId), first.relativePath))), first.sha256);
  assert.equal(sha256(await readFile(path.join(h.store.jobDirectory(h.job.jobId), outputs[0].relativePath))), sha256(changed));
  await assert.rejects(h.store.writeArtifacts({ ...h.job, revision: 2 }, [{
    logicalName: "overwrite", mediaType: "audio/wav", contentBase64: wav.toString("base64")
  }]));
});
