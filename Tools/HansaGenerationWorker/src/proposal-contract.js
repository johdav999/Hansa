import { canonicalJson, hashDocument } from "./canonical-json.js";
import { WorkerError } from "./errors.js";

const SAFE_ID = /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/;
const HASH = /^[0-9a-f]{16,64}$/;
const JSON_TYPES = new Set(["string", "integer", "number", "boolean", "array", "object"]);

function reject(code, message, remedy = "Regenerate the proposal from the current deterministic Editor schema and selected definition.") {
  throw new WorkerError(code, message, { remedy, retryable: false });
}

function requireObject(value, name) {
  if (!value || typeof value !== "object" || Array.isArray(value)) reject("InvalidProposalContract", `${name} must be an object.`);
  return value;
}

function requireExactKeys(value, allowed, name) {
  const unknown = Object.keys(value).filter((key) => !allowed.has(key));
  if (unknown.length > 0) reject("InvalidProposalContract", `${name} contains unknown fields: ${unknown.join(", ")}.`);
}

function requireString(value, name, pattern = undefined, maximum = 256) {
  if (typeof value !== "string" || value.length === 0 || value.length > maximum || (pattern && !pattern.test(value))) {
    reject("InvalidProposalContract", `${name} is invalid.`);
  }
  return value;
}

function cloneFieldSchema(raw, path, depth = 0) {
  if (depth > 6) reject("InvalidProposalContract", `${path} exceeds the maximum schema depth.`);
  const schema = requireObject(raw, path);
  requireExactKeys(schema, new Set(["type", "title", "description", "minimum", "maximum", "enum", "items", "properties", "required", "additionalProperties", "x-hansa-reference"]), path);
  const type = requireString(schema.type, `${path}.type`, undefined, 16);
  if (!JSON_TYPES.has(type)) reject("InvalidProposalContract", `${path}.type is unsupported.`);
  const result = { type };
  if (typeof schema.title === "string") result.title = schema.title.slice(0, 256);
  if (typeof schema.description === "string") result.description = schema.description.slice(0, 1024);
  if (typeof schema["x-hansa-reference"] === "string") result["x-hansa-reference"] = schema["x-hansa-reference"].slice(0, 128);
  if (schema.minimum !== undefined) {
    if (!Number.isFinite(schema.minimum)) reject("InvalidProposalContract", `${path}.minimum is invalid.`);
    result.minimum = schema.minimum;
  }
  if (schema.maximum !== undefined) {
    if (!Number.isFinite(schema.maximum)) reject("InvalidProposalContract", `${path}.maximum is invalid.`);
    result.maximum = schema.maximum;
  }
  if (schema.enum !== undefined) {
    if (!Array.isArray(schema.enum) || schema.enum.length === 0 || schema.enum.length > 128 || schema.enum.some((item) => typeof item !== "string")) {
      reject("InvalidProposalContract", `${path}.enum is invalid.`);
    }
    result.enum = [...schema.enum];
  }
  if (type === "array") result.items = cloneFieldSchema(schema.items, `${path}.items`, depth + 1);
  if (type === "object") {
    const properties = requireObject(schema.properties, `${path}.properties`);
    if (Object.keys(properties).length > 64) reject("InvalidProposalContract", `${path} has too many properties.`);
    result.properties = Object.fromEntries(Object.keys(properties).sort().map((key) => {
      requireString(key, `${path}.property`, SAFE_ID, 128);
      return [key, cloneFieldSchema(properties[key], `${path}.properties.${key}`, depth + 1)];
    }));
    result.required = Object.keys(result.properties);
    result.additionalProperties = false;
  }
  return result;
}

function modelFieldSchema(schema, includeHansaExtensions) {
  const result = { ...schema };
  if (!includeHansaExtensions) delete result["x-hansa-reference"];
  if (result.items) result.items = modelFieldSchema(result.items, includeHansaExtensions);
  if (result.properties) result.properties = Object.fromEntries(Object.entries(result.properties).map(([name, child]) => [name, modelFieldSchema(child, includeHansaExtensions)]));
  return result;
}

function nullable(schema) {
  return { anyOf: [schema, { type: "null" }] };
}

export function normalizeDefinitionProposalContract(request) {
  if (request.capability !== "StructuredDataDraft" || request.intendedAssetRole !== "DefinitionPatch") {
    reject("ProviderRequestRejected", "OpenAI definition proposals require StructuredDataDraft and intendedAssetRole=DefinitionPatch.");
  }
  const raw = requireObject(request.parameters?.definitionProposal, "parameters.definitionProposal");
  requireExactKeys(raw, new Set([
    "contractVersion", "schemaId", "schemaVersion", "sourceSchemaHash", "baseStableId", "baseRevision",
    "baseContentHash", "writableFields", "baseValues", "stableReferences", "maxOutputTokens",
  ]), "parameters.definitionProposal");
  if (raw.contractVersion !== 1) reject("InvalidProposalContract", "definitionProposal.contractVersion must be 1.");
  const schemaId = requireString(raw.schemaId, "definitionProposal.schemaId", SAFE_ID);
  const schemaVersion = raw.schemaVersion;
  const baseRevision = raw.baseRevision;
  if (!Number.isSafeInteger(schemaVersion) || schemaVersion < 1 || !Number.isSafeInteger(baseRevision) || baseRevision < 1) {
    reject("InvalidProposalContract", "Schema and base revisions must be positive integers.");
  }
  const sourceSchemaHash = requireString(raw.sourceSchemaHash, "definitionProposal.sourceSchemaHash", /^[0-9a-f]{64}$/);
  const baseStableId = requireString(raw.baseStableId, "definitionProposal.baseStableId", SAFE_ID);
  const baseContentHash = requireString(raw.baseContentHash, "definitionProposal.baseContentHash", HASH);
  const writableFieldsRaw = requireObject(raw.writableFields, "definitionProposal.writableFields");
  const names = Object.keys(writableFieldsRaw).sort();
  if (names.length === 0 || names.length > 128) reject("InvalidProposalContract", "The proposal must contain 1 through 128 writable fields.");
  const writableFields = Object.fromEntries(names.map((name) => {
    requireString(name, "definitionProposal.writableField", SAFE_ID);
    return [name, cloneFieldSchema(writableFieldsRaw[name], `definitionProposal.writableFields.${name}`)];
  }));
  if (!Array.isArray(raw.stableReferences) || raw.stableReferences.length > 4096) reject("InvalidProposalContract", "stableReferences must be a bounded array.");
  const stableReferences = [...new Set(raw.stableReferences.map((value, index) => requireString(value, `stableReferences[${index}]`, SAFE_ID)))].sort();
  const baseValuesRaw = requireObject(raw.baseValues, "definitionProposal.baseValues");
  requireExactKeys(baseValuesRaw, new Set(names), "definitionProposal.baseValues");
  for (const name of names) {
    if (!Object.hasOwn(baseValuesRaw, name)) reject("InvalidProposalContract", `definitionProposal.baseValues is missing ${name}.`);
    validateValue(baseValuesRaw[name], writableFields[name], `definitionProposal.baseValues.${name}`, new Set(stableReferences));
  }
  const baseValues = Object.fromEntries(names.map((name) => [name, structuredClone(baseValuesRaw[name])]));
  const maxOutputTokens = raw.maxOutputTokens ?? 2048;
  if (!Number.isSafeInteger(maxOutputTokens) || maxOutputTokens < 64 || maxOutputTokens > 16384) reject("InvalidProposalContract", "maxOutputTokens must be from 64 through 16384.");
  const contract = { contractVersion: 1, schemaId, schemaVersion, sourceSchemaHash, baseStableId, baseRevision, baseContentHash, writableFields, baseValues, stableReferences, maxOutputTokens };
  return { ...contract, contractHash: hashDocument(contract) };
}

export function buildDefinitionProposalResponseSchema(contract, includeHansaExtensions = false) {
  const patchProperties = Object.fromEntries(Object.entries(contract.writableFields).map(([name, schema]) => [name, nullable(modelFieldSchema(schema, includeHansaExtensions))]));
  return {
    type: "object",
    additionalProperties: false,
    required: ["proposalVersion", "schemaId", "schemaVersion", "sourceSchemaHash", "baseStableId", "baseRevision", "baseContentHash", "patch"],
    properties: {
      proposalVersion: { type: "integer", enum: [1] },
      schemaId: { type: "string", enum: [contract.schemaId] },
      schemaVersion: { type: "integer", enum: [contract.schemaVersion] },
      sourceSchemaHash: { type: "string", enum: [contract.sourceSchemaHash] },
      baseStableId: { type: "string", enum: [contract.baseStableId] },
      baseRevision: { type: "integer", enum: [contract.baseRevision] },
      baseContentHash: { type: "string", enum: [contract.baseContentHash] },
      patch: { type: "object", additionalProperties: false, required: Object.keys(patchProperties), properties: patchProperties },
    },
  };
}

function validateValue(value, schema, path, references) {
  if (schema.anyOf) {
    if (value === null) return;
    return validateValue(value, schema.anyOf[0], path, references);
  }
  if (schema.enum && !schema.enum.includes(value)) reject("SchemaMismatch", `${path} is outside the schema enum.`);
  const actual = Array.isArray(value) ? "array" : value === null ? "null" : Number.isInteger(value) ? "integer" : typeof value;
  if (schema.type === "number" ? typeof value !== "number" || !Number.isFinite(value) : actual !== schema.type) reject("SchemaMismatch", `${path} has type ${actual}; expected ${schema.type}.`);
  if (schema.type === "integer" && !Number.isSafeInteger(value)) reject("SchemaMismatch", `${path} must be a safely representable JSON integer.`);
  if (typeof value === "number" && ((schema.minimum !== undefined && value < schema.minimum) || (schema.maximum !== undefined && value > schema.maximum))) reject("SchemaMismatch", `${path} is outside the allowed range.`);
  if (schema["x-hansa-reference"] && schema["x-hansa-reference"] !== "None" && typeof value === "string" && value && !references.has(value)) reject("UnknownStableReference", `${path} references unknown stable ID ${value}.`);
  if (schema.type === "array") value.forEach((item, index) => validateValue(item, schema.items, `${path}[${index}]`, references));
  if (schema.type === "object") {
    const unknown = Object.keys(value).filter((key) => !Object.hasOwn(schema.properties, key));
    if (unknown.length > 0) reject("UnknownProposalField", `${path} contains unknown fields: ${unknown.join(", ")}.`);
    for (const key of schema.required ?? []) if (!Object.hasOwn(value, key)) reject("SchemaMismatch", `${path}.${key} is required.`);
    for (const [key, child] of Object.entries(schema.properties)) validateValue(value[key], child, `${path}.${key}`, references);
  }
}

export function validateDefinitionProposal(value, contract) {
  if (!value || typeof value !== "object" || Array.isArray(value)) reject("MalformedProviderOutput", "Structured output must be one JSON object.");
  const schema = buildDefinitionProposalResponseSchema(contract, true);
  validateValue(value, schema, "proposal", new Set(contract.stableReferences));
  const changedFields = Object.entries(value.patch).filter(([, item]) => item !== null);
  if (changedFields.length === 0) reject("EmptyProposal", "The structured proposal did not change any writable field.");
  return { proposal: value, changedFields: changedFields.map(([name]) => name), contractHash: contract.contractHash, proposalHash: hashDocument(value), canonicalJson: canonicalJson(value) };
}
