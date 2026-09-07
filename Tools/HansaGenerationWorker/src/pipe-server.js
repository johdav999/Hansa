import net from "node:net";
import { FrameDecoder, encodeFrame } from "./protocol.js";
import { structuredError, WorkerError } from "./errors.js";

export function pipePath(pipeName) {
  if (typeof pipeName !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/.test(pipeName)) {
    throw new WorkerError("InvalidPipeName", "Pipe name must be a bounded local identifier.");
  }
  return process.platform === "win32" ? `\\\\.\\pipe\\${pipeName}` : `/tmp/${pipeName}.sock`;
}

export class WorkerPipeServer {
  constructor({ pipeName, handler, logger }) {
    this.path = pipePath(pipeName);
    this.handler = handler;
    this.logger = logger;
    this.sockets = new Set();
    this.server = net.createServer((socket) => this.#accept(socket));
  }

  #accept(socket) {
    this.sockets.add(socket);
    const decoder = new FrameDecoder();
    let chain = Promise.resolve();
    socket.on("data", (chunk) => {
      chain = chain.then(async () => {
        for (const request of decoder.push(chunk)) socket.write(encodeFrame(await this.handler.handle(request)));
      }).catch((error) => {
        this.logger?.log("warning", "protocol.connection-error", { error: structuredError(error) });
        socket.destroy();
      });
    });
    socket.on("close", () => this.sockets.delete(socket));
    socket.on("error", (error) => this.logger?.log("warning", "protocol.socket-error", { code: error.code }));
  }

  async listen() {
    await new Promise((resolve, reject) => {
      this.server.once("error", reject);
      this.server.listen(this.path, () => {
        this.server.off("error", reject);
        resolve();
      });
    });
    this.logger?.log("info", "worker.listening", { transport: "local-named-pipe" });
  }

  async close() {
    for (const socket of this.sockets) socket.destroy();
    if (!this.server.listening) return;
    await new Promise((resolve) => this.server.close(resolve));
  }
}
