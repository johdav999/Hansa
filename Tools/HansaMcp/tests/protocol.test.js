import assert from "node:assert/strict";
import test from "node:test";
import { FrameDecoder, HansaWireError, MAX_FRAME_BYTES, encodeFrame } from "../src/protocol.js";

test("framing accepts fragmented and coalesced UTF-8 messages", () => {
  const first = encodeFrame({ value: "Lübeck" });
  const second = encodeFrame({ value: 2 });
  const decoder = new FrameDecoder();
  assert.deepEqual(decoder.push(first.subarray(0, 2)), []);
  assert.deepEqual(decoder.push(Buffer.concat([first.subarray(2), second])), [
    { value: "Lübeck" },
    { value: 2 },
  ]);
});

test("framing rejects zero and oversized payloads", () => {
  const decoder = new FrameDecoder();
  assert.throws(() => decoder.push(Buffer.alloc(4)), HansaWireError);
  assert.throws(() => encodeFrame({ value: "x".repeat(MAX_FRAME_BYTES) }), HansaWireError);
});
