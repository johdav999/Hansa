import { MAX_FRAME_BYTES, PROTOCOL_NAME, PROTOCOL_VERSION } from "./contracts.js";
import { WorkerError } from "./errors.js";

export function encodeFrame(value) {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  if (payload.length === 0 || payload.length > MAX_FRAME_BYTES) throw new WorkerError("FrameTooLarge", "Protocol frame exceeds the 1 MiB limit.");
  const frame = Buffer.allocUnsafe(payload.length + 4);
  frame.writeUInt32LE(payload.length, 0);
  payload.copy(frame, 4);
  return frame;
}

export class FrameDecoder {
  constructor() {
    this.buffer = Buffer.alloc(0);
  }

  push(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    const messages = [];
    while (this.buffer.length >= 4) {
      const length = this.buffer.readUInt32LE(0);
      if (length === 0 || length > MAX_FRAME_BYTES) throw new WorkerError("InvalidFrame", "Protocol frame length is invalid.");
      if (this.buffer.length < length + 4) break;
      const payload = this.buffer.subarray(4, length + 4);
      this.buffer = this.buffer.subarray(length + 4);
      try {
        messages.push(JSON.parse(payload.toString("utf8")));
      } catch {
        throw new WorkerError("InvalidJson", "Protocol frame does not contain valid UTF-8 JSON.");
      }
    }
    return messages;
  }
}

export function validateEnvelope(request, authenticationToken) {
  if (!request || typeof request !== "object" || Array.isArray(request)) throw new WorkerError("InvalidEnvelope", "Request envelope must be an object.");
  if (request.protocol !== PROTOCOL_NAME || request.version !== PROTOCOL_VERSION) {
    throw new WorkerError("UnsupportedProtocol", `Expected ${PROTOCOL_NAME} protocol version ${PROTOCOL_VERSION}.`);
  }
  if (typeof request.requestId !== "string" || !/^[A-Za-z0-9-]{8,128}$/.test(request.requestId)) throw new WorkerError("InvalidEnvelope", "requestId is invalid.");
  if (request.authToken !== authenticationToken) {
    throw new WorkerError("AuthenticationFailed", "Worker authentication failed.", { remedy: "Launch editor and worker with the same short-lived local token." });
  }
  if (typeof request.operation !== "string") throw new WorkerError("InvalidEnvelope", "operation is required.");
}
