import net from "node:net";
import { randomUUID } from "node:crypto";
import { PROTOCOL_NAME, PROTOCOL_VERSION } from "./contracts.js";
import { FrameDecoder, encodeFrame } from "./protocol.js";
import { pipePath } from "./pipe-server.js";
import { WorkerError } from "./errors.js";

export class WorkerPipeClient {
  constructor({ pipeName, authenticationToken, timeoutMs = 5_000 }) {
    this.path = pipePath(pipeName);
    this.authenticationToken = authenticationToken;
    this.timeoutMs = timeoutMs;
  }

  async call(operation, payload = {}) {
    const socket = net.createConnection(this.path);
    const decoder = new FrameDecoder();
    const requestId = randomUUID();
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        socket.destroy();
        reject(new WorkerError("ProtocolTimeout", `Worker operation ${operation} timed out.`, { retryable: true }));
      }, this.timeoutMs);
      socket.once("connect", () => socket.write(encodeFrame({
        protocol: PROTOCOL_NAME, version: PROTOCOL_VERSION, requestId,
        authToken: this.authenticationToken, operation, payload,
      })));
      socket.on("data", (chunk) => {
        try {
          for (const response of decoder.push(chunk)) {
            if (response.requestId !== requestId) continue;
            clearTimeout(timer);
            socket.end();
            if (!response.ok) reject(new WorkerError(response.error.code, response.error.message, response.error));
            else resolve(response.result);
          }
        } catch (error) {
          clearTimeout(timer);
          socket.destroy();
          reject(error);
        }
      });
      socket.once("error", (error) => {
        clearTimeout(timer);
        reject(error);
      });
    });
  }
}
