/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

module.exports = async function ({
  targetCommand,
  targetFront,
  onAvailableArray,
}) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, i.e. still ignore them in
  // the context of the browser toolbox as we inspect messages via the process
  // targets
  const listenForFrames = targetCommand.descriptorFront.isTabDescriptor;

  // Allow workers when messages aren't dispatched to the main thread.
  const listenForWorkers =
    !targetCommand.rootFront.traits
      .workerConsoleApiMessagesDispatchedToMainThread;

  const acceptTarget =
    targetFront.isTopLevel ||
    targetFront.targetType === targetCommand.TYPES.PROCESS ||
    (targetFront.targetType === targetCommand.TYPES.FRAME && listenForFrames) ||
    (targetFront.targetType === targetCommand.TYPES.WORKER && listenForWorkers);

  if (!acceptTarget) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  if (webConsoleFront.isDestroyed()) {
    return;
  }

  // Request notifying about new messages
  await webConsoleFront.startListeners(["ConsoleAPI"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners(ConsoleAPI) first /!\
  const { messages } = await webConsoleFront.getCachedMessages(["ConsoleAPI"]);

  onAvailableArray([
    [
      ResourceCommand.TYPES.CONSOLE_MESSAGE,
      messages.map(({ message }) => message),
    ],
  ]);

  // Forward new message events
  webConsoleFront.on("consoleAPICall", ({ message }) => {
    // Ignore console messages that are cloned from the content process
    // (they aren't relevant to toolboxes still using legacy listeners)
    if (message.clonedFromContentProcess) {
      return;
    }

    onAvailableArray([[ResourceCommand.TYPES.CONSOLE_MESSAGE, [message]]]);
  });
};
