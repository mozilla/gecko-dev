/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",

  error: "chrome://remote/content/shared/messagehandler/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  RootMessageHandlerRegistry:
    "chrome://remote/content/shared/messagehandler/RootMessageHandlerRegistry.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

ChromeUtils.defineLazyGetter(lazy, "WebDriverError", () => {
  return ChromeUtils.importESModule(
    "chrome://remote/content/shared/webdriver/Errors.sys.mjs"
  ).error.WebDriverError;
});

// Set the timeout delay before a command is considered as potentially timing
// out. This can be customized by a preference mostly for tests. Regular
// implementation should use DEFAULT_COMMAND_DELAY;
const DEFAULT_COMMAND_DELAY = 10000;
const PREF_REMOTE_COMMAND_DELAY = "remote.messagehandler.test.command.delay";

ChromeUtils.defineLazyGetter(lazy, "commandDelay", () =>
  Services.prefs.getIntPref(PREF_REMOTE_COMMAND_DELAY, DEFAULT_COMMAND_DELAY)
);

const PING_DELAY = 1000;
const PING_TIMEOUT = Symbol();

/**
 * Parent actor for the MessageHandlerFrame JSWindowActor. The
 * MessageHandlerFrame actor is used by RootTransport to communicate between
 * ROOT MessageHandlers and WINDOW_GLOBAL MessageHandlers.
 */
export class MessageHandlerFrameParent extends JSWindowActorParent {
  #destroyed;

  constructor() {
    super();
    this.#destroyed = false;
  }

  didDestroy() {
    this.#destroyed = true;
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "MessageHandlerFrameChild:sendCommand": {
        return this.#handleSendCommandMessage(message.data);
      }
      case "MessageHandlerFrameChild:messageHandlerEvent": {
        return this.#handleMessageHandlerEventMessage(message.data);
      }
      default:
        throw new Error("Unsupported message:" + message.name);
    }
  }

  /**
   * Send a command to the corresponding MessageHandlerFrameChild actor via a
   * JSWindowActor query.
   *
   * @param {Command} command
   *     The command to forward. See type definition in MessageHandler.js
   * @param {string} sessionId
   *     ID of the session that sent the command.
   * @returns {Promise}
   *     Promise that will resolve with the result of query sent to the
   *     MessageHandlerFrameChild actor.
   */
  async sendCommand(command, sessionId) {
    const timer = lazy.setTimeout(
      () => this.#sendPing(command),
      lazy.commandDelay
    );

    const result = await this.sendQuery(
      "MessageHandlerFrameParent:sendCommand",
      {
        command,
        sessionId,
      }
    );

    lazy.clearTimeout(timer);

    if (result?.error) {
      if (result.isMessageHandlerError) {
        throw lazy.error.MessageHandlerError.fromJSON(result.error);
      }

      // TODO: Do not assume WebDriver is the session protocol, see Bug 1779026.
      throw lazy.WebDriverError.fromJSON(result.error);
    }

    return result;
  }

  async #handleMessageHandlerEventMessage(messageData) {
    const { name, contextInfo, data, sessionId } = messageData;
    const [moduleName] = name.split(".");

    // Re-emit the event on the RootMessageHandler.
    const messageHandler =
      lazy.RootMessageHandlerRegistry.getExistingMessageHandler(sessionId);
    // TODO: getModuleInstance expects a CommandDestination in theory,
    // but only uses the MessageHandler type in practice, see Bug 1776389.
    const module = messageHandler.moduleCache.getModuleInstance(moduleName, {
      type: lazy.WindowGlobalMessageHandler.type,
    });
    let eventPayload = data;

    // Modify an event payload if there is a special method in the targeted module.
    // If present it can be found in windowglobal-in-root module.
    if (module?.interceptEvent) {
      eventPayload = await module.interceptEvent(name, data);

      if (eventPayload === null) {
        lazy.logger.trace(
          `${moduleName}.interceptEvent returned null, skipping event: ${name}, data: ${data}`
        );
        return;
      }
      // Make sure that an event payload is returned.
      if (!eventPayload) {
        throw new Error(
          `${moduleName}.interceptEvent doesn't return the event payload`
        );
      }
    }
    messageHandler.emitEvent(name, eventPayload, contextInfo);
  }

  async #handleSendCommandMessage(messageData) {
    const { sessionId, command } = messageData;
    const messageHandler =
      lazy.RootMessageHandlerRegistry.getExistingMessageHandler(sessionId);
    try {
      return await messageHandler.handleCommand(command);
    } catch (e) {
      if (e?.isRemoteError) {
        return {
          error: e.toJSON(),
          isMessageHandlerError: e.isMessageHandlerError,
        };
      }
      throw e;
    }
  }

  async #sendPing(command) {
    const commandName = `${command.moduleName}.${command.commandName}`;
    const destination = command.destination.id;

    if (this.#destroyed) {
      // If the JSWindowActor was destroyed already, no need to send a ping.
      return;
    }

    lazy.logger.trace(
      `MessageHandlerFrameParent command ${commandName} to ${destination} ` +
        `takes more than ${lazy.commandDelay / 1000} seconds to resolve, sending ping`
    );

    try {
      const result = await Promise.race([
        this.sendQuery("MessageHandlerFrameParent:sendPing"),
        new Promise(r => lazy.setTimeout(() => r(PING_TIMEOUT), PING_DELAY)),
      ]);

      if (result === PING_TIMEOUT) {
        lazy.logger.warn(
          `MessageHandlerFrameParent ping for command ${commandName} to ${destination} timed out`
        );
      } else {
        lazy.logger.trace(
          `MessageHandlerFrameParent ping for command ${commandName} to ${destination} was successful`
        );
      }
    } catch (e) {
      if (!this.#destroyed) {
        // Only swallow errors if the JSWindowActor pair was destroyed while
        // waiting for the ping response.
        throw e;
      }

      lazy.logger.trace(
        `MessageHandlerFrameParent ping for command ${commandName} to ${destination}` +
          ` lost after JSWindowActor was destroyed`
      );
    }
  }
}
