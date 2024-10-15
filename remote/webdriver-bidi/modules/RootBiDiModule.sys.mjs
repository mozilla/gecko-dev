/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

export class RootBiDiModule extends Module {
  /**
   * Emits an event for a specific browsing context.
   *
   * @param {string} browsingContextId
   *     The ID of the browsing context to which the event should be emitted.
   * @param {string} eventName
   *     The name of the event to be emitted.
   * @param {object} eventPayload
   *     The payload to be sent with the event.
   * @returns {boolean}
   *     Returns `true` if the event was successfully emitted, otherwise `false`.
   */
  _emitEventForBrowsingContext(browsingContextId, eventName, eventPayload) {
    // This event is emitted from the parent process but for a given browsing
    // context. Set the event's contextInfo to the message handler corresponding
    // to this browsing context.
    const contextInfo = {
      contextId: browsingContextId,
      type: lazy.WindowGlobalMessageHandler.type,
    };
    return this.emitEvent(eventName, eventPayload, contextInfo);
  }

  /**
   * Checks if there is a listener for a specific event and context information.
   *
   * @param {string} eventName
   *     The name of the event to check for listeners.
   * @param {ContextInfo} contextInfo
   *     The context information to check for listeners within.
   * @returns {boolean}
   *     Returns `true` if there is at least one listener for the specified event and context, otherwise `false`.
   */
  _hasListener(eventName, contextInfo) {
    return this.messageHandler.eventsDispatcher.hasListener(
      eventName,
      contextInfo
    );
  }

  /**
   * Forwards a command to the windowglobal module corresponding to the provided
   * browsing context id, using the same module name as the current one.
   *
   * @param {string} commandName
   *     The name of the command to execute.
   * @param {number} browsingContextID
   *     The debuggable context ID.
   * @param {object} params
   *    Any command parameters to pass.
   * @param {object=} args
   *     Any additional command arguments to pass.
   * @returns {Promise}
   *     A Promise that will resolve with the return value of the
   *     command once it has been executed.
   */
  _forwardToWindowGlobal(commandName, browsingContextID, params, args = {}) {
    return this.messageHandler.forwardCommand({
      moduleName: this.moduleName,
      commandName,
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: browsingContextID,
      },
      ...args,
      params,
    });
  }
}
