import assert from "node:assert/strict";
import test from "node:test";
import { Writable } from "node:stream";
import { createLogger } from "../src/logger.js";

test("logger redacts secret keys and known secret values", () => {
  let output = "";
  const stream = new Writable({ write(chunk, encoding, callback) { output += chunk.toString(); callback(); } });
  const logger = createLogger({ stream, secrets: ["short-lived-token"] });
  logger.log("warn", "test", {
    authenticationToken: "short-lived-token",
    message: "failed for short-lived-token",
    nested: { password: "do-not-log" },
  });
  assert.doesNotMatch(output, /short-lived-token|do-not-log/);
  assert.match(output, /\[REDACTED\]/);
});
