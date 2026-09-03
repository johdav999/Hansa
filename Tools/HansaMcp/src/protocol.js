export const WIRE_SCHEMA_VERSION = 1;
export const MAX_FRAME_BYTES = 64 * 1024;
export const DEFAULT_TIMEOUT_MS = 5_000;

export class HansaWireError extends Error {
  constructor(code, message, { correlationId = "", remedy = "", retryable = false } = {}) {
    super(message);
    this.name = "HansaWireError";
    this.code = code;
    this.correlationId = correlationId;
    this.remedy = remedy;
    this.retryable = retryable;
  }

  toJSON() {
    return {
      code: this.code,
      correlationId: this.correlationId,
      message: this.message,
      remedy: this.remedy,
      retryable: this.retryable,
    };
  }
}

export function encodeFrame(value) {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  if (payload.length === 0 || payload.length > MAX_FRAME_BYTES) {
    throw new HansaWireError("InvalidFrame", `Frame payload must be between 1 and ${MAX_FRAME_BYTES} bytes.`);
  }
  const header = Buffer.allocUnsafe(4);
  header.writeUInt32LE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

export class FrameDecoder {
  #buffer = Buffer.alloc(0);

  push(chunk) {
    if (!Buffer.isBuffer(chunk)) {
      chunk = Buffer.from(chunk);
    }
    this.#buffer = Buffer.concat([this.#buffer, chunk]);
    const messages = [];
    while (this.#buffer.length >= 4) {
      const payloadLength = this.#buffer.readUInt32LE(0);
      if (payloadLength === 0 || payloadLength > MAX_FRAME_BYTES) {
        throw new HansaWireError("InvalidFrame", "Frame length is zero or exceeds the 64 KiB limit.");
      }
      if (this.#buffer.length < payloadLength + 4) {
        break;
      }
      const payload = this.#buffer.subarray(4, payloadLength + 4);
      this.#buffer = this.#buffer.subarray(payloadLength + 4);
      try {
        messages.push(JSON.parse(payload.toString("utf8")));
      } catch {
        throw new HansaWireError("InvalidFrame", "Frame payload is not valid UTF-8 JSON.");
      }
    }
    if (this.#buffer.length > MAX_FRAME_BYTES + 4) {
      throw new HansaWireError("InvalidFrame", "Framed input exceeded the bounded receive buffer.");
    }
    return messages;
  }
}

export function assertWireResponse(response, requestId) {
  if (!response || typeof response !== "object" || response.schemaVersion !== WIRE_SCHEMA_VERSION || response.requestId !== requestId || typeof response.ok !== "boolean") {
    throw new HansaWireError("InvalidResponse", "The game returned an invalid or mismatched wire response.", {
      correlationId: requestId,
      remedy: "Restart the game and sidecar with matching protocol versions.",
    });
  }
  if (!response.ok) {
    const error = response.error ?? {};
    throw new HansaWireError(error.code ?? "RemoteError", error.message ?? "The game rejected the request.", {
      correlationId: error.correlationId ?? requestId,
      remedy: error.remedy ?? "Inspect the structured error and retry only when marked retryable.",
      retryable: error.retryable === true,
    });
  }
  return response.payload ?? {};
}
