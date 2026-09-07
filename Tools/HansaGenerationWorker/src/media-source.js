// Provider-neutral media source retention. Unreal owns import and promotion.
import path from "node:path";
import { mkdir, readFile, writeFile, lstat } from "node:fs/promises";
import { canonicalJson, hashDocument, sha256 } from "./canonical-json.js";
import { WorkerError } from "./errors.js";

import { validateStaticPropGlb } from "./static-prop.js";

import { validateAudioTake } from "./audio-take.js";

const formats = new Map([["model/gltf-binary", "glb"], ["audio/wav", "wav"]]);
function requireThat(value, message) {
  if (!value) throw new WorkerError("MediaSourceRejected", message);
}

// Reject containers that can resolve external files. Deeper mesh/audio QA is
// editor/provider-profile owned; this is the shared pre-import safety gate.
export function validateMediaBytes(bytes, mediaType) {
  requireThat(Buffer.isBuffer(bytes) && bytes.length > 0 && bytes.length <= 64 * 1024 * 1024, "Media must contain 1 through 67108864 bytes.");
  requireThat(formats.has(mediaType), "Only self-contained GLB 2 and PCM WAV are accepted by media contract v1.");
  if (mediaType === "model/gltf-binary") {
    requireThat(bytes.length >= 20 && bytes.toString("ascii", 0, 4) === "glTF" && bytes.readUInt32LE(4) === 2 && bytes.readUInt32LE(8) === bytes.length, "Invalid GLB 2 header or length.");
    let offset = 12;
    let document;
    let binaryLength = 0;
    while (offset < bytes.length) {
      requireThat(offset + 8 <= bytes.length, "Truncated GLB chunk.");
      const length = bytes.readUInt32LE(offset), type = bytes.readUInt32LE(offset + 4);
      requireThat(length % 4 === 0 && offset + 8 + length <= bytes.length, "Invalid GLB chunk length.");
      if (offset === 12) {
        requireThat(type === 0x4e4f534a, "GLB must begin with JSON.");
        try { document = JSON.parse(bytes.toString("utf8", offset + 8, offset + 8 + length)); }
        catch { throw new WorkerError("MediaSourceRejected", "Invalid GLB JSON."); }
      } else {
        requireThat(type === 0x004e4942 && binaryLength === 0, "Only one embedded BIN chunk is allowed.");
        binaryLength = length;
      }
      offset += 8 + length;
    }
    requireThat(document?.asset?.version === "2.0" && Array.isArray(document.meshes) && document.meshes.length > 0, "GLB requires a versioned mesh document.");
    requireThat(!document.extensionsRequired?.length && !document.extensionsUsed?.length, "GLB extensions require a separately reviewed import contract.");
    requireThat(!document.skins?.length && !document.animations?.length, "Media contract v1 accepts static meshes only.");
    requireThat(Array.isArray(document.buffers) && document.buffers.length === 1 && !document.buffers[0].uri && Number.isSafeInteger(document.buffers[0].byteLength) && document.buffers[0].byteLength > 0 && document.buffers[0].byteLength <= binaryLength, "GLB must use one embedded buffer.");
    function inspect(value) {
      if (!value || typeof value !== "object") return;
      for (const [key, child] of Object.entries(value)) {
        requireThat(key !== "uri" && key !== "extensions", "External URI or extension content is prohibited.");
        inspect(child);
      }
    }
    inspect(document);
    return { format: "glb2", meshCount: document.meshes.length, embedded: true };
  }
  requireThat(bytes.length >= 44 && bytes.toString("ascii", 0, 4) === "RIFF" && bytes.toString("ascii", 8, 12) === "WAVE" && bytes.readUInt32LE(4) + 8 === bytes.length, "Invalid WAV RIFF header or length.");
  let offset = 12, fmt, data;
  while (offset < bytes.length) {
    requireThat(offset + 8 <= bytes.length, "Truncated WAV chunk.");
    const name = bytes.toString("ascii", offset, offset + 4), length = bytes.readUInt32LE(offset + 4);
    requireThat(offset + 8 + length + (length % 2) <= bytes.length, "Invalid WAV chunk length.");
    if (name === "fmt ") { requireThat(!fmt && length === 16, "WAV requires one PCM fmt chunk."); fmt = bytes.subarray(offset + 8, offset + 8 + length); }
    else if (name === "data") { requireThat(!data && length > 0, "WAV requires one nonempty data chunk."); data = bytes.subarray(offset + 8, offset + 8 + length); }
    else requireThat(name === "JUNK" || name === "LIST", "Unsupported WAV chunk.");
    offset += 8 + length + (length % 2);
  }
  requireThat(fmt && data && fmt.readUInt16LE(0) === 1 && fmt.readUInt16LE(14) === 16, "WAV must contain 16-bit PCM audio.");
  const channels = fmt.readUInt16LE(2), sampleRate = fmt.readUInt32LE(4), alignment = fmt.readUInt16LE(12);
  requireThat([1, 2].includes(channels) && [22050, 24000, 44100, 48000].includes(sampleRate) && alignment === channels * 2 && fmt.readUInt32LE(8) === sampleRate * alignment && data.length % alignment === 0, "Invalid WAV channel, sample-rate or frame contract.");
  return { format: "pcm16", channels, sampleRate, durationSeconds: data.length / alignment / sampleRate };
}

async function immutable(file, bytes) {
  try { await writeFile(file, bytes, { flag: "wx", flush: true }); }
  catch (error) {
    if (error.code !== "EEXIST") throw error;
    requireThat((await lstat(file)).isFile() && !(await lstat(file)).isSymbolicLink(), "Source file must be a regular file.");
    requireThat(sha256(await readFile(file)) === sha256(bytes), "Immutable source collision; preserve it and create a new revision.");
  }
}

// Check every existing path component to prevent junction/symlink escape.
async function safeDirectory(root, relative) {
  let current = path.resolve(root);
  for (const part of relative.split("/")) {
    current = path.join(current, part);
    await mkdir(current).catch(error => { if (error.code !== "EEXIST") throw error; });
    const info = await lstat(current);
    requireThat(info.isDirectory() && !info.isSymbolicLink(), "Source retention cannot follow a link or junction.");
  }
  return current;
}

export async function retainMediaSource(store, projectRoot, jobId, outputIndex) {
  requireThat(projectRoot, "Media source export is not configured for this worker.");
  const job = await store.getJob(jobId);
  requireThat(job.status === "Review" && Number.isSafeInteger(outputIndex) && outputIndex >= 0, "Select a Review-state media output.");
  const output = job.outputs[outputIndex];
  const extension = formats.get(output?.mediaType);
  requireThat(extension && new RegExp(`^output/${job.revision === 1 ? "" : `r${job.revision}/`}artifact-[0-9]{3}\\.${extension}$`).test(output.relativePath), "Output is not an allowlisted media artifact.");
  const record = job.manifests.find(item => item.revision === job.revision);
  requireThat(record && record.relativePath === `manifests/manifest-r${job.revision}.json`, "Completed immutable manifest is required.");
  const manifest = JSON.parse(await readFile(path.join(store.jobDirectory(jobId), record.relativePath), "utf8"));
  const { manifestHash, ...body } = manifest;
  requireThat(hashDocument(body) === manifestHash && manifestHash === record.manifestHash && body.status === "Review" && body.jobId === jobId && body.revision === job.revision && hashDocument(body.outputs) === hashDocument(job.outputs) && body.requestHash === job.requestHash, "Completed manifest integrity failed.");
  requireThat(body.provider?.providerId && body.provider?.modelVersion && body.provenance?.adapterVersion && body.rights?.acknowledged === true && body.qaResults?.length && body.qaResults.every(check => check.passed === true), "Provider, rights and passing validation provenance are required.");
  const bytes = await readFile(path.join(store.jobDirectory(jobId), output.relativePath));
  requireThat(bytes.length === output.sizeBytes && sha256(bytes) === output.sha256, "Source output hash or size mismatch.");
  const validation = validateMediaBytes(bytes, output.mediaType);
  if (body.provider.providerId === "tripo" || body.parameters?.staticProp) {
    requireThat(extension === "glb", "Static props require GLB.");
    validation.staticProp = validateStaticPropGlb(bytes, body.parameters?.staticProp);
  }
  let original;
  if (body.provider.providerId === "elevenlabs" || body.parameters?.audioTake) {
    requireThat(extension === "wav", "AudioTake requires WAV.");
    validation.audioTake = validateAudioTake(bytes, body.parameters?.audioTake);
    if (body.provider.providerId === "elevenlabs") {
      original = body.provenance?.takes?.[outputIndex]?.source;
      requireThat(original && original.mediaType === "audio/mpeg" && original.relativePath === `output/${job.revision === 1 ? "" : "r" + job.revision + "/"}original-${String(outputIndex + 1).padStart(3, "0")}.mp3`, "Missing original provider audio provenance.");
    }
  }
  const domain = extension === "glb" ? "Meshes" : "Audio";
  const relativeDirectory = `SourceArt/Generated/${domain}/${jobId}/r${job.revision}/artifact-${outputIndex + 1}`;
  const directory = await safeDirectory(projectRoot, relativeDirectory);
  await immutable(path.join(directory, `source.${extension}`), bytes);
  if (original) {
    const originalBytes = await readFile(path.join(store.jobDirectory(jobId), original.relativePath));
    requireThat(originalBytes.length <= 2 * 1024 * 1024 && originalBytes.length === original.sizeBytes && sha256(originalBytes) === original.sha256, "Original provider source integrity failed.");
    await immutable(path.join(directory, "provider-original.mp3"), originalBytes);
    validation.providerOriginal = { sha256: original.sha256, sizeBytes: original.sizeBytes, relativePath: "provider-original.mp3" };
  }
  await immutable(path.join(directory, "job-manifest.json"), Buffer.from(`${canonicalJson(manifest)}\n`));
  const source = { schemaVersion: 1, jobId, revision: job.revision, outputIndex, domain, mediaType: output.mediaType, sha256: output.sha256, sizeBytes: output.sizeBytes, manifestHash, manifestFileSha256: sha256(`${canonicalJson(manifest)}\n`), sourcePath: `${relativeDirectory}/source.${extension}`, manifestPath: `${relativeDirectory}/job-manifest.json`, validation };
  await immutable(path.join(directory, "source.json"), Buffer.from(`${canonicalJson(source)}\n`));
  return { ...source, descriptorPath: `${relativeDirectory}/source.json` };
}
