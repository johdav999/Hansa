import { hashDocument, sha256 } from "./canonical-json.js";
import { WorkerError } from "./errors.js";
import { containsSensitiveKey, redactText, redactValue } from "./redaction.js";

const SAFE_ID = /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/;
const CAPABILITIES = new Set([
  "StructuredDataDraft", "TextToMesh", "ImageToMesh", "MultiViewToMesh", "MeshVariation",
  "TextureGeneration", "Retopology", "Segmentation", "RigCheck", "AutoRig",
  "AnimationPreset", "AnimationRetarget", "TextToSpeech", "TextToDialogue", "TextToSoundEffect",
]);

function requireString(value, name, { maximum = 1024, pattern = undefined, allowEmpty = false } = {}) {
  if (typeof value !== "string" || (!allowEmpty && value.length === 0) || value.length > maximum || (pattern && !pattern.test(value))) {
    throw new WorkerError("InvalidRequest", `${name} is invalid.`, {
      remedy: `Provide ${name} as a bounded string matching the worker contract.`,
    });
  }
  return value;
}

function requireInteger(value, name, minimum, maximum) {
  if (!Number.isSafeInteger(value) || value < minimum || value > maximum) {
    throw new WorkerError("InvalidRequest", `${name} must be an integer from ${minimum} through ${maximum}.`);
  }
  return value;
}

function decodeBase64(value, name) {
  requireString(value, name, { maximum: 8 * 1024 * 1024, pattern: /^[A-Za-z0-9+/]*={0,2}$/ });
  const bytes = Buffer.from(value, "base64");
  if (bytes.toString("base64").replace(/=+$/, "") !== value.replace(/=+$/, "")) {
    throw new WorkerError("InvalidRequest", `${name} is not canonical base64.`);
  }
  return bytes;
}

export function normalizeGenerationRequest(raw, { requireSpendApproval = true } = {}) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new WorkerError("InvalidRequest", "The generation request must be an object.");
  }
  if (containsSensitiveKey(raw)) {
    throw new WorkerError("ForbiddenSensitiveField", "Generation requests cannot contain credentials or authorization material.", {
      remedy: "Provide credentials only through the worker environment or approved OS credential store.",
    });
  }
  const allowedRootFields = new Set([
    "schemaVersion", "idempotencyKey", "capability", "intendedAssetRole", "providerId", "modelVersion",
    "prompt", "negativePrompt", "inputArtifacts", "rights", "budgets", "outputContract", "seed",
    "parameters", "timeoutMs", "spendApproval", "privacy",
  ]);
  const unknownFields = Object.keys(raw).filter((key) => !allowedRootFields.has(key));
  if (unknownFields.length > 0) {
    throw new WorkerError("InvalidRequest", `Unknown generation request fields: ${unknownFields.join(", ")}.`);
  }
  if (raw.schemaVersion !== 1) {
    throw new WorkerError("UnsupportedSchemaVersion", "Generation request schemaVersion must be 1.");
  }

  const capability = requireString(raw.capability, "capability", { maximum: 64, pattern: SAFE_ID });
  if (!CAPABILITIES.has(capability)) {
    throw new WorkerError("UnsupportedCapability", `Unknown provider-neutral capability: ${capability}.`);
  }
  const prompt = requireString(raw.prompt, "prompt", { maximum: 16_384 });
  const negativePrompt = raw.negativePrompt === undefined ? "" : requireString(raw.negativePrompt, "negativePrompt", { maximum: 8_192, allowEmpty: true });
  const timeoutMs = requireInteger(raw.timeoutMs ?? 30_000, "timeoutMs", 25, 300_000);
  const seed = raw.seed === undefined ? null : requireInteger(raw.seed, "seed", 0, Number.MAX_SAFE_INTEGER);
  const idempotencyKey = requireString(raw.idempotencyKey, "idempotencyKey", { maximum: 128, pattern: SAFE_ID });
  const providerId = requireString(raw.providerId, "providerId", { maximum: 64, pattern: SAFE_ID });
  const modelVersion = requireString(raw.modelVersion, "modelVersion", { maximum: 128, pattern: SAFE_ID });
  const intendedAssetRole = requireString(raw.intendedAssetRole, "intendedAssetRole", { maximum: 128, pattern: SAFE_ID });

  if (requireSpendApproval && (!raw.spendApproval || raw.spendApproval.approved !== true)) {
    throw new WorkerError("SpendApprovalRequired", "A generation job requires explicit spend approval before queueing.", {
      remedy: "Estimate the request, identify the approver, and submit spendApproval.approved=true.",
    });
  }
  const spendApproval = raw.spendApproval?.approved === true ? {
    approved: true,
    approvedBy: requireString(raw.spendApproval.approvedBy, "spendApproval.approvedBy", { maximum: 128 }),
    approvedAt: requireString(raw.spendApproval.approvedAt, "spendApproval.approvedAt", { maximum: 64 }),
  } : { approved: false };

  const budgets = {
    maximumCostMinorUnits: requireInteger(raw.budgets?.maximumCostMinorUnits ?? 0, "budgets.maximumCostMinorUnits", 0, 1_000_000_000),
    currency: requireString(raw.budgets?.currency ?? "USD", "budgets.currency", { maximum: 8, pattern: /^[A-Z]{3}$/ }),
    maximumOutputBytes: requireInteger(raw.budgets?.maximumOutputBytes ?? 1_048_576, "budgets.maximumOutputBytes", 1, 256 * 1024 * 1024),
  };
  const outputContract = {
    version: requireInteger(raw.outputContract?.version ?? 1, "outputContract.version", 1, 1),
    maximumArtifacts: requireInteger(raw.outputContract?.maximumArtifacts ?? 1, "outputContract.maximumArtifacts", 1, 16),
    allowedMediaTypes: Array.isArray(raw.outputContract?.allowedMediaTypes) && raw.outputContract.allowedMediaTypes.length > 0
      ? raw.outputContract.allowedMediaTypes.map((item, index) => requireString(item, `outputContract.allowedMediaTypes[${index}]`, { maximum: 128, pattern: /^[a-z0-9.+-]+\/[a-z0-9.+-]+$/ }))
      : ["application/json"],
  };

  const privateInputs = [];
  if (!Array.isArray(raw.inputArtifacts ?? []) || (raw.inputArtifacts ?? []).length > 16) {
    throw new WorkerError("InvalidRequest", "inputArtifacts must contain at most 16 items.");
  }
  const inputArtifacts = (raw.inputArtifacts ?? []).map((input, index) => {
    if (!input || typeof input !== "object" || Array.isArray(input)) {
      throw new WorkerError("InvalidRequest", `inputArtifacts[${index}] must be an object.`);
    }
    const bytes = decodeBase64(input.contentBase64, `inputArtifacts[${index}].contentBase64`);
    if (bytes.length > 16 * 1024 * 1024) {
      throw new WorkerError("InputTooLarge", `inputArtifacts[${index}] exceeds 16 MiB.`);
    }
    const descriptor = {
      index,
      role: requireString(input.role, `inputArtifacts[${index}].role`, { maximum: 64, pattern: SAFE_ID }),
      mediaType: requireString(input.mediaType, `inputArtifacts[${index}].mediaType`, { maximum: 128, pattern: /^[a-z0-9.+-]+\/[a-z0-9.+-]+$/ }),
      sizeBytes: bytes.length,
      sha256: sha256(bytes),
      rightsDeclaration: requireString(input.rightsDeclaration, `inputArtifacts[${index}].rightsDeclaration`, { maximum: 512 }),
    };
    privateInputs.push({ index, bytes });
    return descriptor;
  });

  const promptPrivate = raw.privacy?.promptPrivate === true;
  const safePrompt = promptPrivate ? `[REDACTED_PRIVATE_INPUT sha256=${sha256(prompt)}]` : redactText(prompt);
  const safeNegativePrompt = promptPrivate && negativePrompt
    ? `[REDACTED_PRIVATE_INPUT sha256=${sha256(negativePrompt)}]`
    : redactText(negativePrompt);
  const parameters = redactValue(raw.parameters && typeof raw.parameters === "object" && !Array.isArray(raw.parameters) ? raw.parameters : {});
  const safeRequest = {
    schemaVersion: 1,
    idempotencyKeyHash: sha256(idempotencyKey),
    capability,
    intendedAssetRole,
    providerId,
    modelVersion,
    prompt: safePrompt,
    promptSha256: sha256(prompt),
    negativePrompt: safeNegativePrompt,
    negativePromptSha256: sha256(negativePrompt),
    inputArtifacts,
    rights: redactValue(raw.rights && typeof raw.rights === "object" ? raw.rights : {}),
    budgets,
    outputContract,
    seed,
    parameters,
    timeoutMs,
    spendApproval,
  };
  return {
    idempotencyKeyHash: safeRequest.idempotencyKeyHash,
    safeRequest,
    privateInputs,
    requestHash: hashDocument(safeRequest),
    inputHash: hashDocument(inputArtifacts),
  };
}

