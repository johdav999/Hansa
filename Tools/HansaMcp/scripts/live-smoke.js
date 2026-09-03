#!/usr/bin/env node

import { HansaAutomationClient } from "../src/client.js";
import { createLogger } from "../src/logger.js";
import { NamedPipeTransport } from "../src/transport.js";

const authenticationToken = process.env.HANSA_AUTOMATION_TOKEN ?? "";
const logger = createLogger({ secrets: [authenticationToken] });
const transport = new NamedPipeTransport({
  pipeName: process.env.HANSA_AUTOMATION_PIPE,
  logger,
  connectAttempts: 80,
  initialBackoffMs: 100,
  maximumBackoffMs: 500,
});
const client = new HansaAutomationClient({
  transport,
  authenticationToken,
  controllerId: "hansa-mcp-live-smoke",
});

try {
  const ping = await client.ping();
  const capabilities = await client.capabilitiesGet();
  const session = await client.sessionStart({ requestedPermission: "ControlledActions" });
  const health = await client.health();
  const activated = await client.uiActivate("AutomationProof.Activate");
  const activatedWait = await client.waitFor({ semanticId: "AutomationProof.Activate", property: "selected", timeoutMs: 1_000 });
  const focused = await client.uiFocus("AutomationProof.FocusTarget");
  const focusedWait = await client.waitFor({ semanticId: "AutomationProof.FocusTarget", property: "focused", timeoutMs: 1_000 });
  const warning = await client.uiState("AutomationProof.Warning");
  const wait = await client.waitFor({ semanticId: "AutomationProof.Warning", property: "warning", timeoutMs: 1_000 });
  const capture720 = await client.captureScreenshot({ width: 1280, height: 720, bundleId: "live-smoke-720" });
  const capture1080 = await client.captureScreenshot({ width: 1920, height: 1080, bundleId: "live-smoke-1080" });
  await client.sessionStop();
  process.stdout.write(`${JSON.stringify({
    status: "succeeded",
    ping,
    protocolVersion: capabilities.protocolVersion,
    capabilities: capabilities.capabilities.map(({ name }) => name),
    sessionId: session.sessionId,
    health,
    activated: { id: activated.node.id, state: activated.node.state },
    activatedWait,
    focused: { id: focused.node.id, state: focused.node.state },
    focusedWait,
    warning: { id: warning.node.id, role: warning.node.role, state: warning.node.state },
    wait,
    captures: [capture720, capture1080],
  })}\n`);
} finally {
  transport.close();
}
