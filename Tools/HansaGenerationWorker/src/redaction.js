const SENSITIVE_KEY = /(api[-_]?key|access[-_]?token|refresh[-_]?token|auth(?:entication)?[-_]?token|bearer[-_]?token|client[-_]?secret|secret[-_]?key|password|authorization|credentials?|cookies?|privateinput|privatepath)/i;
const URL_PATTERN = /\b(?:https?|wss?):\/\/[^\s"'<>]+/gi;
const BEARER_PATTERN = /\bBearer\s+[A-Za-z0-9._~+/=-]+/gi;

export function redactText(value, secrets = []) {
  let result = String(value).replace(URL_PATTERN, "[REDACTED_URL]").replace(BEARER_PATTERN, "Bearer [REDACTED]");
  for (const secret of secrets) {
    if (secret) result = result.replaceAll(secret, "[REDACTED]");
  }
  return result;
}

export function redactValue(value, secrets = []) {
  if (Array.isArray(value)) return value.map((item) => redactValue(item, secrets));
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.entries(value).map(([key, item]) => [
      key,
      isSensitiveKey(key) ? "[REDACTED]" : redactValue(item, secrets),
    ]));
  }
  return typeof value === "string" ? redactText(value, secrets) : value;
}

export function containsSensitiveKey(value) {
  if (!value || typeof value !== "object") return false;
  return Object.entries(value).some(([key, item]) => isSensitiveKey(key) || containsSensitiveKey(item));
}

function isSensitiveKey(key) {
  const normalized = key.replaceAll("-", "").replaceAll("_", "").toLowerCase();
  if (/^(?:max(?:imum)?)?outputtokens?$/.test(normalized)) return false;
  return normalized === "secret" || normalized.endsWith("token") || SENSITIVE_KEY.test(key);
}

export function createLogger({ stream = process.stderr, secrets = [] } = {}) {
  return {
    log(level, event, details = {}) {
      const record = redactValue({ timestamp: new Date().toISOString(), level, event, ...details }, secrets);
      stream.write(`${JSON.stringify(record)}\n`);
    },
  };
}
