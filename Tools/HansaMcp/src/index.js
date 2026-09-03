#!/usr/bin/env node

import { HansaAutomationClient } from "./client.js";
import { FakeHansaEndpoint, FakeInProcessTransport } from "./fake-endpoint.js";
import { createLogger } from "./logger.js";
import { HansaMcpServer, serveStdio } from "./mcp-server.js";
import { NamedPipeTransport } from "./transport.js";

function argumentValue(name) {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : undefined;
}

const useFake = process.argv.includes("--fake");
const authenticationToken = process.env.HANSA_AUTOMATION_TOKEN ?? (useFake ? "hansa-test-token-1234" : "");
const logger = createLogger({ secrets: [authenticationToken] });
const transport = useFake
  ? new FakeInProcessTransport(new FakeHansaEndpoint({ authenticationToken }))
  : new NamedPipeTransport({
      pipeName: argumentValue("--pipe") ?? process.env.HANSA_AUTOMATION_PIPE,
      logger,
    });
const client = new HansaAutomationClient({
  transport,
  authenticationToken,
  controllerId: argumentValue("--controller-id") ?? process.env.HANSA_AUTOMATION_CONTROLLER_ID ?? "hansa-mcp",
});
const server = new HansaMcpServer({ client, logger });

const shutdown = () => transport.close();
process.once("SIGINT", shutdown);
process.once("SIGTERM", shutdown);
process.once("exit", shutdown);

await serveStdio(server);
shutdown();
