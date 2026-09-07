#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { createWriteStream, cpSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import process from "node:process";
import { HansaAutomationClient } from "../src/client.js";
import { createLogger } from "../src/logger.js";
import { NamedPipeTransport } from "../src/transport.js";

const FIXTURE_ID = "two_player_authority_v1";
const TOKEN = process.env.HANSA_AUTOMATION_TOKEN ?? "";
const CONFIG_PATH = process.argv[2];

if (!CONFIG_PATH) throw new Error("Pass the generated authority-proof config path.");
const config = JSON.parse(readFileSync(CONFIG_PATH, "utf8"));
const artifactDirectory = path.resolve(config.artifactDirectory);
mkdirSync(artifactDirectory, { recursive: true });

const processes = new Map();
const endpoints = new Map();
const correlations = [];
let timedOut = false;

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

function remainingDeadline(deadline) {
  const remaining = deadline - Date.now();
  if (remaining <= 0) {
    timedOut = true;
    throw new Error("The two-player authority proof exceeded its bounded timeout.");
  }
  return remaining;
}

function processArguments(role, logPath) {
  const common = [
    "-game",
    "-Multiprocess",
    "-unattended",
    "-nop4",
    "-nosplash",
    "-NoSound",
    "-HansaAuthorityFixture",
    "-HansaAutomation",
    "-HansaAutomationPermission=FixtureControl",
    "-AbsLog=" + logPath,
  ];
  if (role === "server") {
    return [
      config.projectFile,
      config.map + "?listen?Scenario=lubeck_grain_shortage_v1?CampaignSeed=" + config.seed,
      "-server",
      "-port=" + config.port,
      "-NullRHI",
      ...common,
    ];
  }
  return [
    config.projectFile,
    "127.0.0.1:" + config.port,

    "-Windowed",
    "-ResX=1280",
    "-ResY=720",
    ...common,
  ];
}

function startProcess(name, role, pipeName) {
  const logPath = path.join(artifactDirectory, name + ".unreal.log");
  const streamPath = path.join(artifactDirectory, name + ".launcher.log");
  const output = createWriteStream(streamPath, { flags: "w" });
  const child = spawn(config.unrealEditor, processArguments(role, logPath), {
    cwd: config.projectRoot,
    env: {
      ...process.env,
      HANSA_AUTOMATION_TOKEN: TOKEN,
      HANSA_AUTOMATION_PIPE: pipeName,
      UE_SKIP_UBT_SDK_SETUP: "1",
    },
    windowsHide: true,
    stdio: ["ignore", "pipe", "pipe"],
  });
  child.stdout.pipe(output);
  child.stderr.pipe(output);
  processes.set(name, { child, role, pipeName, logPath, streamPath, output });
  return child;
}

async function stopProcess(name) {
  const entry = processes.get(name);
  if (!entry) return;
  if (entry.child.exitCode === null) {
    entry.child.kill();
    const exited = new Promise((resolve) => entry.child.once("exit", resolve));
    await Promise.race([exited, delay(5_000)]);
    if (entry.child.exitCode === null) entry.child.kill("SIGKILL");
  }
  entry.output.end();
}

async function connectEndpoint(name, pipeName, deadline) {
  const logger = createLogger({ secrets: [TOKEN] });
  let lastError;
  while (remainingDeadline(deadline) > 0) {
    const processEntry = processes.get(name);
    if (processEntry && processEntry.child.exitCode !== null) {
      throw new Error(name + " exited before its automation endpoint became ready (exit " + processEntry.child.exitCode + ").");
    }
    const transport = new NamedPipeTransport({
      pipeName,
      logger,
      connectAttempts: 1,
      initialBackoffMs: 100,
      maximumBackoffMs: 250,
    });
    const client = new HansaAutomationClient({
      transport,
      authenticationToken: TOKEN,
      controllerId: "s11-p04-" + name,
      timeoutMs: 5_000,
    });
    try {
      await client.ping();
      await client.sessionStart({
        requestedPermission: "FixtureControl",
        requiredCapabilities: [
          "session",
          "capabilities",
          "health",
          "gameplay.query",
          "gameplay.command",
          "fixture.control",
          "screenshots",
          "wait-assertions",
        ],
      });
      endpoints.set(name, { client, transport });
      return client;
    } catch (error) {
      lastError = error;
      transport.close();
      await delay(250);
    }
  }
  throw lastError ?? new Error(name + " endpoint did not become ready.");
}

async function activateFixture(client, deadline) {
  let lastError;
  while (remainingDeadline(deadline) > 0) {
    try {
      return await client.fixtureLoad(FIXTURE_ID);
    } catch (error) {
      lastError = error;
      await delay(200);
    }
  }
  throw lastError ?? new Error("Fixture activation timed out.");
}

async function status(client) {
  const value = await client.gameplayQuery("multiplayer.status");
  correlations.push(value.correlationId);
  return value;
}

async function waitStatus(client, predicate, description, deadline) {
  let latest;
  while (remainingDeadline(deadline) > 0) {
    latest = await status(client);
    if (predicate(latest)) return latest;
    await delay(100);
  }
  throw new Error("Timed out waiting for " + description + ". Latest: " + JSON.stringify(latest));
}

async function submitAndWait(client, parameters, expected, deadline) {
  const submitted = await client.gameplayCommand(parameters.command, parameters);
  correlations.push(submitted.correlationId);
  const observed = await waitStatus(
    client,
    (value) => value.lastCommandFeedback && value.lastCommandFeedback.clientSequence === parameters.clientSequence,
    "feedback for client sequence " + parameters.clientSequence,
    deadline,
  );
  assert.equal(observed.lastCommandFeedback.accepted, expected.accepted);
  if (expected.rejection) {
    assert.equal(observed.lastCommandFeedback.rejection, expected.rejection);
    assert.ok(observed.lastCommandFeedback.remedy);
  }
  if (expected.accepted) {
    assert.ok(observed.lastCommandFeedback.acceptedGlobalSequence > 0);
  }
  return { submitted, observed };
}

function route(statusValue, routeId) {
  return statusValue.routes.find((item) => item.routeId === routeId);
}

function copyCapture(capture, label) {
  const destination = path.join(artifactDirectory, "captures", label);
  mkdirSync(destination, { recursive: true });
  const copied = {};
  for (const [key, source] of Object.entries(capture)) {
    if (!key.endsWith("Path") || typeof source !== "string" || !existsSync(source)) continue;
    const target = path.join(destination, path.basename(source));
    cpSync(source, target);
    copied[key] = target;
  }
  return { ...capture, copied };
}

async function closeEndpoint(name) {
  const endpoint = endpoints.get(name);
  if (!endpoint) return;
  try {
    if (endpoint.client.sessionId) await endpoint.client.sessionStop();
  } catch {}
  endpoint.transport.close();
  endpoints.delete(name);
}

async function runProof(deadline) {
  const pipes = config.pipes;
  startProcess("server", "server", pipes.server);
  const server = await connectEndpoint("server", pipes.server, deadline);
  await activateFixture(server, deadline);
  await waitStatus(server, (value) => value.ready === true, "server fixture readiness", deadline);

  startProcess("client1", "client", pipes.client1);
  const client1 = await connectEndpoint("client1", pipes.client1, deadline);
  await activateFixture(client1, deadline);
  const initial1 = await waitStatus(client1, (value) => value.ready === true, "client 1 readiness", deadline);

  startProcess("client2", "client", pipes.client2);
  const client2 = await connectEndpoint("client2", pipes.client2, deadline);
  await activateFixture(client2, deadline);
  const initial2 = await waitStatus(client2, (value) => value.ready === true, "client 2 readiness", deadline);
  const serverReady = await waitStatus(
    server,
    (value) => value.registeredClientCount === 2 && value.publicAuthoritativeHash,
    "two registered clients",
    deadline,
  );
  assert.deepEqual([initial1.publicHouseId, initial2.publicHouseId], [1, 2]);

  const placed = await submitAndWait(client1, {
    command: "multiplayer.place_building",
    clientSequence: 1,
    clientNonce: 11001,
    cityId: "City.Lubeck",
    buildingDefinitionId: "Building.Road",
    anchorX: 10,
    anchorY: 30,
  }, { accepted: true }, deadline);
  const firstRejected = await submitAndWait(client1, {
    command: "multiplayer.set_route_active",
    clientSequence: 2,
    clientNonce: 11002,
    routeId: 3,
    active: true,
  }, { accepted: false, rejection: "NotAuthorized" }, deadline);
  const secondRejected = await submitAndWait(client2, {
    command: "multiplayer.set_route_active",
    clientSequence: 1,
    clientNonce: 22001,
    routeId: 1,
    active: true,
  }, { accepted: false, rejection: "NotAuthorized" }, deadline);
  const secondAllowed = await submitAndWait(client2, {
    command: "multiplayer.set_route_active",
    clientSequence: 2,
    clientNonce: 22002,
    routeId: 3,
    active: true,
  }, { accepted: true }, deadline);

  let synchronized = {};
  while (remainingDeadline(deadline) > 0) {
    synchronized = {
      server: await status(server),
      client1: await status(client1),
      client2: await status(client2),
    };
    const hash = synchronized.server.publicAuthoritativeHash;
    if (hash &&
      synchronized.client1.authoritativeHash === hash &&
      synchronized.client2.authoritativeHash === hash &&
      synchronized.client1.placements.some((item) =>
        item.cityId === "City.Lubeck" && item.buildingDefinitionId === "Building.Road") &&
      route(synchronized.client2, 3)?.cargoVisible === true &&
      route(synchronized.client1, 3)?.cargoVisible === false) {
      break;
    }
    await delay(100);
  }
  assert.equal(synchronized.client1.authoritativeHash, synchronized.server.publicAuthoritativeHash);
  assert.equal(synchronized.client2.authoritativeHash, synchronized.server.publicAuthoritativeHash);
  assert.notEqual(synchronized.client1.projectionDigest, synchronized.client2.projectionDigest);
  assert.equal(route(synchronized.client1, 3)?.cargoVisible, false);
  assert.equal(route(synchronized.client2, 3)?.cargoVisible, true);

  const capture1 = copyCapture(
    await client1.captureScreenshot({ width: 1280, height: 720, bundleId: "client1-authority" }),
    "client1",
  );
  const capture2 = copyCapture(
    await client2.captureScreenshot({ width: 1280, height: 720, bundleId: "client2-authority" }),
    "client2",
  );

  const disconnectedHouse = synchronized.client2.publicHouseId;
  const beforeReconnectHash = synchronized.server.publicAuthoritativeHash;
  await closeEndpoint("client2");
  await stopProcess("client2");
  const serverAfterDisconnect = await waitStatus(
    server,
    (value) => value.registeredClientCount === 1,
    "server disconnect observation",
    deadline,
  );

  startProcess("client2-reconnect", "client", pipes.reconnect);
  const reconnectedClient = await connectEndpoint("client2-reconnect", pipes.reconnect, deadline);
  await activateFixture(reconnectedClient, deadline);
  const reconnected = await waitStatus(
    reconnectedClient,
    (value) => value.ready === true && value.fullRefresh === true &&
      value.publicHouseId === disconnectedHouse &&
      value.authoritativeHash === beforeReconnectHash,
    "full reconnect projection resynchronization",
    deadline,
  );
  const serverAfterReconnect = await waitStatus(
    server,
    (value) => value.registeredClientCount === 2 &&
      value.publicAuthoritativeHash === beforeReconnectHash,
    "server reconnect observation",
    deadline,
  );
  assert.equal(route(reconnected, 3)?.ownerHouseId, 2);
  assert.equal(route(reconnected, 3)?.cargoVisible, true);

  const reconnectCapture = copyCapture(
    await reconnectedClient.captureScreenshot({
      width: 1920,
      height: 1080,
      bundleId: "client2-reconnected",
    }),
    "client2-reconnect",
  );

  const result = {
    operation: "RunTwoPlayerAuthorityProof",
    status: "Succeeded",
    fixtureId: FIXTURE_ID,
    completedUtc: new Date().toISOString(),
    port: config.port,
    processLogs: Object.fromEntries([...processes.entries()].map(([name, entry]) => [
      name,
      { pid: entry.child.pid, unrealLog: entry.logPath, launcherLog: entry.streamPath },
    ])),
    correlations: [...new Set(correlations.filter(Boolean))],
    readiness: { server: serverReady, client1: initial1, client2: initial2 },
    commands: { placed, firstRejected, secondRejected, secondAllowed },
    synchronized,
    disconnect: serverAfterDisconnect,
    reconnect: { server: serverAfterReconnect, client: reconnected },
    diagnostics: {
      authoritativeHash: beforeReconnectHash,
      clientProjectionDigests: {
        client1: synchronized.client1.projectionDigest,
        client2: synchronized.client2.projectionDigest,
        reconnected: reconnected.projectionDigest,
      },
    },
    screenshots: [capture1, capture2, reconnectCapture],
    timedOut,
  };
  writeFileSync(path.join(artifactDirectory, "result.json"), JSON.stringify(result, null, 2) + "\n");
  process.stdout.write(JSON.stringify({
    status: result.status,
    authoritativeHash: result.diagnostics.authoritativeHash,
    projectionDigests: result.diagnostics.clientProjectionDigests,
    artifactDirectory,
  }) + "\n");
}

const timeoutMilliseconds = Math.max(10_000, Number(config.timeoutSeconds ?? 90) * 1_000);
const deadline = Date.now() + timeoutMilliseconds;
let failure;
try {
  await runProof(deadline);
} catch (error) {
  failure = error;
  writeFileSync(path.join(artifactDirectory, "failure.json"), JSON.stringify({
    operation: "RunTwoPlayerAuthorityProof",
    status: "Failed",
    completedUtc: new Date().toISOString(),
    error: String(error?.stack ?? error),
    timedOut,
    processes: Object.fromEntries([...processes.entries()].map(([name, entry]) => [
      name,
      { pid: entry.child.pid, exitCode: entry.child.exitCode, unrealLog: entry.logPath, launcherLog: entry.streamPath },
    ])),
  }, null, 2) + "\n");
} finally {
  for (const name of [...endpoints.keys()]) await closeEndpoint(name);
  for (const name of [...processes.keys()].reverse()) await stopProcess(name);
}
if (failure) throw failure;