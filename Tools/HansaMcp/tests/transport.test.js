import assert from "node:assert/strict";
import net from "node:net";
import test from "node:test";
import { randomUUID } from "node:crypto";
import { FrameDecoder, encodeFrame } from "../src/protocol.js";
import { NamedPipeTransport, windowsPipePath } from "../src/transport.js";

test("named-pipe transport waits for delayed endpoint availability", { skip: process.platform !== "win32" }, async (t) => {
  const pipeName = `hansa-mcp-test-${randomUUID()}`;
  const path = windowsPipePath(pipeName);
  const server = net.createServer((socket) => {
    const decoder = new FrameDecoder();
    socket.on("data", (chunk) => {
      for (const request of decoder.push(chunk)) {
        socket.write(encodeFrame({ schemaVersion: 1, requestId: request.requestId, ok: true, payload: { pong: true } }));
      }
    });
  });
  t.after(() => server.close());
  setTimeout(() => server.listen(path), 70);

  const transport = new NamedPipeTransport({
    pipeName,
    connectAttempts: 8,
    initialBackoffMs: 20,
    maximumBackoffMs: 40,
  });
  t.after(() => transport.close());
  const response = await transport.request({ schemaVersion: 1, requestId: "retry-test", operation: "ping", timeoutMs: 1_000, payload: {} });
  assert.equal(response.payload.pong, true);
});

test("named-pipe path rejects unsafe or broad names", () => {
  assert.throws(() => windowsPipePath("../unsafe"));
  assert.equal(windowsPipePath("hansa.safe-1"), "\\\\.\\pipe\\hansa.safe-1");
});
