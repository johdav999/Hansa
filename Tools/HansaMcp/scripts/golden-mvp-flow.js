import { HansaAutomationClient } from "../src/client.js";
import { runGoldenMvpFlow } from "../src/golden-flow.js";
import { NamedPipeTransport } from "../src/transport.js";

const client = new HansaAutomationClient({
  transport: new NamedPipeTransport({ pipeName: process.env.HANSA_AUTOMATION_PIPE }),
  authenticationToken: process.env.HANSA_AUTOMATION_TOKEN,
  controllerId: "s14-p01-mvp-golden",
});

try {
  const result = await runGoldenMvpFlow(client);
  process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
} catch (error) {
  process.stderr.write(`${JSON.stringify({
    ok: false,
    error: error.goldenFailure ?? { phase: "sidecar", checkpoint: "unexpected", message: error.message },
  }, null, 2)}\n`);
  process.exitCode = 1;
} finally {
  client.transport.close();
}
