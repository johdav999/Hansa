const SECRET_KEY = /(token|secret|password|authorization|credential)/i;

function redactValue(value, secrets) {
  if (Array.isArray(value)) {
    return value.map((item) => redactValue(item, secrets));
  }
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.entries(value).map(([key, item]) => [
      key,
      SECRET_KEY.test(key) ? "[REDACTED]" : redactValue(item, secrets),
    ]));
  }
  if (typeof value === "string") {
    return secrets.reduce((text, secret) => secret ? text.replaceAll(secret, "[REDACTED]") : text, value);
  }
  return value;
}

export function createLogger({ stream = process.stderr, secrets = [] } = {}) {
  return {
    log(level, event, details = {}) {
      const record = redactValue({
        timestamp: new Date().toISOString(),
        level,
        event,
        ...details,
      }, secrets);
      stream.write(`${JSON.stringify(record)}\n`);
    },
  };
}

export { redactValue };
