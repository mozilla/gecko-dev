/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "./firefox/commands";
import { setupEvents, clientEvents } from "./firefox/events";
import { features, prefs } from "../utils/prefs";
import type { Grip } from "../types";
let DebuggerClient;

function createObjectFront(grip: Grip) {
  return DebuggerClient.createObjectFront(grip);
}

export async function onConnect(connection: any, actions: Object) {
  const {
    tabConnection: { tabTarget, threadFront, debuggerClient },
  } = connection;

  DebuggerClient = debuggerClient;

  if (!tabTarget || !threadFront || !debuggerClient) {
    return;
  }

  setupCommands({
    threadFront,
    tabTarget,
    debuggerClient,
  });

  setupEvents({ threadFront, tabTarget, actions, debuggerClient });

  tabTarget.on("will-navigate", actions.willNavigate);
  tabTarget.on("navigate", actions.navigated);

  const wasmBinarySource =
    features.wasm && !!debuggerClient.mainRoot.traits.wasmBinarySource;

  await threadFront.reconfigure({
    observeAsmJS: true,
    pauseWorkersUntilAttach: true,
    wasmBinarySource,
    skipBreakpoints: prefs.skipPausing,
    logEventBreakpoints: prefs.logEventBreakpoints,
  });

  // Retrieve possible event listener breakpoints
  actions.getEventListenerBreakpointTypes().catch(e => console.error(e));

  // Initialize the event breakpoints on the thread up front so that
  // they are active once attached.
  actions.addEventListenerBreakpoints([]).catch(e => console.error(e));

  const { traits } = tabTarget;
  await actions.connect(
    tabTarget.url,
    threadFront.actor,
    traits,
    tabTarget.isWebExtension
  );

  // Fetch the sources for all the targets
  //
  // In Firefox, we need to initially request all of the sources. This
  // usually fires off individual `newSource` notifications as the
  // debugger finds them, but there may be existing sources already in
  // the debugger (if it's paused already, or if loading the page from
  // bfcache) so explicity fire `newSource` events for all returned
  // sources.
  const sources = await clientCommands.fetchSources();
  await actions.newGeneratedSources(sources);

  await clientCommands.checkIfAlreadyPaused();
}

export { createObjectFront, clientCommands, clientEvents };
