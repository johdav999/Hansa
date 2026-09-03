import net from "node:net";
import { FrameDecoder, HansaWireError, encodeFrame } from "./protocol.js";

const PIPE_NAME_PATTERN = /^[A-Za-z0-9._-]{1,64}$/;
const RESPONSE_GRACE_MS = 1_000;

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

export function windowsPipePath(pipeName) {
  if (!PIPE_NAME_PATTERN.test(pipeName ?? "")) {
    throw new HansaWireError("ConfigurationError", "HANSA_AUTOMATION_PIPE must be a 1-64 character local pipe name.", {
      remedy: "Set the same safe per-run HANSA_AUTOMATION_PIPE value for the game and HansaMcp.",
    });
  }
  return `\\\\.\\pipe\\${pipeName}`;
}

export class NamedPipeTransport {
  #socket = null;
  #decoder = new FrameDecoder();
  #pending = null;
  #closed = false;

  constructor({ pipeName, logger, connectAttempts = 8, initialBackoffMs = 50, maximumBackoffMs = 500 }) {
    this.pipeName = pipeName;
    this.logger = logger;
    this.connectAttempts = connectAttempts;
    this.initialBackoffMs = initialBackoffMs;
    this.maximumBackoffMs = maximumBackoffMs;
  }

  async connect() {
    if (this.#closed) {
      throw new HansaWireError("TransportClosed", "The sidecar transport has been shut down.");
    }
    if (this.#socket && !this.#socket.destroyed) {
      return;
    }
    const path = windowsPipePath(this.pipeName);
    let lastError;
    for (let attempt = 1; attempt <= this.connectAttempts; attempt += 1) {
      try {
        this.#socket = await this.#openSocket(path);
        this.#decoder = new FrameDecoder();
        this.logger?.log("info", "transport.connected", { attempt });
        return;
      } catch (error) {
        lastError = error;
        if (attempt < this.connectAttempts) {
          const backoffMs = Math.min(this.initialBackoffMs * (2 ** (attempt - 1)), this.maximumBackoffMs);
          await delay(backoffMs);
        }
      }
    }
    throw new HansaWireError("TransportUnavailable", "The Hansa development endpoint is not accepting connections.", {
      remedy: "Start a non-Shipping game/editor with -HansaAutomation and matching token/pipe environment variables.",
      retryable: true,
      cause: lastError,
    });
  }

  #openSocket(path) {
    return new Promise((resolve, reject) => {
      const socket = net.createConnection(path);
      const fail = (error) => {
        socket.destroy();
        reject(error);
      };
      socket.once("error", fail);
      socket.once("connect", () => {
        socket.off("error", fail);
        socket.on("data", (chunk) => this.#onData(chunk));
        socket.on("error", (error) => this.#onDisconnect(error));
        socket.on("close", () => this.#onDisconnect());
        resolve(socket);
      });
    });
  }

  async request(envelope) {
    if (this.#pending) {
      throw new HansaWireError("ConcurrentRequest", "Only one bounded game request may be in flight per sidecar.");
    }
    await this.connect();
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.#pending = null;
        reject(new HansaWireError("TransportTimeout", "The game did not answer before the request timeout.", {
          correlationId: envelope.requestId,
          remedy: "Inspect game health and retry read-only operations after reconnect.",
          retryable: true,
        }));
      }, envelope.timeoutMs + RESPONSE_GRACE_MS);
      this.#pending = { requestId: envelope.requestId, resolve, reject, timer };
      this.#socket.write(encodeFrame(envelope), (error) => {
        if (error) {
          this.#rejectPending(new HansaWireError("TransportDisconnected", "The request could not be written to the game.", {
            correlationId: envelope.requestId,
            retryable: true,
          }));
        }
      });
    });
  }

  #onData(chunk) {
    try {
      for (const message of this.#decoder.push(chunk)) {
        if (!this.#pending) {
          this.logger?.log("warn", "transport.unmatched_response", { requestId: message?.requestId ?? "unknown" });
          continue;
        }
        const pending = this.#pending;
        this.#pending = null;
        clearTimeout(pending.timer);
        pending.resolve(message);
      }
    } catch (error) {
      this.#onDisconnect(error);
      this.#socket?.destroy();
    }
  }

  #onDisconnect(error) {
    if (this.#socket) {
      this.#socket.removeAllListeners();
      this.#socket = null;
    }
    this.#rejectPending(error instanceof HansaWireError ? error : new HansaWireError(
      "TransportDisconnected",
      "The game endpoint disconnected; no request is retried automatically.",
      { remedy: "Retry a read-only tool. Reconcile session state before retrying a session mutation.", retryable: true },
    ));
    if (!this.#closed) {
      this.logger?.log("warn", "transport.disconnected");
    }
  }

  #rejectPending(error) {
    if (!this.#pending) return;
    const pending = this.#pending;
    this.#pending = null;
    clearTimeout(pending.timer);
    pending.reject(error);
  }

  close() {
    this.#closed = true;
    this.#rejectPending(new HansaWireError("TransportClosed", "The sidecar is shutting down."));
    this.#socket?.destroy();
    this.#socket = null;
  }
}
