/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ADB } = require("devtools/shared/adb/adb");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");
const { ClientWrapper } = require("./client-wrapper");

const { RUNTIMES } = require("../constants");

async function createLocalClient() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();
  return { clientWrapper: new ClientWrapper(client) };
}

async function createNetworkClient(host, port) {
  const transportDetails = { host, port };
  const transport = await DebuggerClient.socketConnect(transportDetails);
  const client = new DebuggerClient(transport);
  await client.connect();
  return { clientWrapper: new ClientWrapper(client), transportDetails };
}

async function createUSBClient(socketPath) {
  const port = await ADB.prepareTCPConnection(socketPath);
  return createNetworkClient("localhost", port);
}

async function createClientForRuntime(runtime) {
  const { extra, type } = runtime;

  if (type === RUNTIMES.THIS_FIREFOX) {
    return createLocalClient();
  } else if (type === RUNTIMES.NETWORK) {
    const { host, port } = extra.connectionParameters;
    return createNetworkClient(host, port);
  } else if (type === RUNTIMES.USB) {
    const { socketPath } = extra.connectionParameters;
    return createUSBClient(socketPath);
  }

  return null;
}

exports.createClientForRuntime = createClientForRuntime;

require("./test-helper").enableMocks(module, "modules/runtime-client-factory");
