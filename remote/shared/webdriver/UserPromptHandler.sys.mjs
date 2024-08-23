/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
});

/**
 * @typedef {string} PromptHandlers
 */

/**
 * Enum of known prompt handlers.
 *
 * @readonly
 * @enum {PromptHandlers}
 *
 * @see https://w3c.github.io/webdriver/#dfn-known-prompt-handlers
 */
export const PromptHandlers = {
  /** All simple dialogs encountered should be accepted. */
  Accept: "accept",
  /**
   * All simple dialogs encountered should be accepted, and an error
   * returned that the dialog was handled.
   */
  AcceptAndNotify: "accept and notify",
  /** All simple dialogs encountered should be dismissed. */
  Dismiss: "dismiss",
  /**
   * All simple dialogs encountered should be dismissed, and an error
   * returned that the dialog was handled.
   */
  DismissAndNotify: "dismiss and notify",
  /** All simple dialogs encountered should be left to the user to handle. */
  Ignore: "ignore",
};

/**
 * @typedef {string} PromptTypes
 */

/**
 * Enum of valid prompt types.
 *
 * @readonly
 * @enum {PromptTypes}
 *
 * @see https://w3c.github.io/webdriver/#dfn-valid-prompt-types
 */
export const PromptTypes = {
  // A simple alert dialog
  Alert: "alert",
  // A simple beforeunload dialog. If no handler is set it falls back to the
  // `accept` handler to keep backward compatibility with WebDriver classic,
  // which doesn't customize this prompt type.
  BeforeUnload: "beforeUnload",
  // A simple confirm dialog
  Confirm: "confirm",
  // Default value when no specific handler is configured. Can only be set when
  // specifying the unhandlePromptBehavior capability with a map containing a
  // "default" entry. See FALLBACK_DEFAULT_PROMPT_TYPE.
  Default: "default",
  // A simple prompt dialog
  Prompt: "prompt",
};

/**
 * Internal prompt type used when the unhandledPromptBehavior capability is
 * set as a string. The "fallbackDefault" type will apply to "alert", "confirm"
 * and "prompt" prompts, but will not apply to "beforeUnload" prompts.
 */
const FALLBACK_DEFAULT_PROMPT_TYPE = "fallbackDefault";

export class PromptHandlerConfiguration {
  #handler;
  #notify;

  /**
   * Create an instance of a prompt handler configuration.
   *
   * @param {string} handler
   *     Handler to use for the user prompt. One of "accept", "dismiss" or "ignore".
   * @param {boolean} notify
   *     Flag to indicate if the user needs to be notified when the dialog was
   *     handled.
   *
   * @see https://w3c.github.io/webdriver/#dfn-prompt-handler-configuration
   */
  constructor(handler, notify) {
    this.#handler = handler;
    this.#notify = notify;
  }

  get handler() {
    return this.#handler;
  }

  get notify() {
    return this.#notify;
  }

  toString() {
    return "[object PromptHandlerConfiguration]";
  }

  /**
   * JSON serialization of the prompt handler configuration object.
   *
   * @returns {Record<string, ?>} json
   *
   * @see https://w3c.github.io/webdriver/#dfn-serialize-a-prompt-handler-configuration
   */
  toJSON() {
    let serialized = this.#handler;
    if (["accept", "dismiss"].includes(serialized) && this.#notify) {
      serialized += " and notify";
    }

    return serialized;
  }
}

export class UserPromptHandler {
  #promptTypeToHandlerMap;

  constructor() {
    // Note: this map is null until update-the-user-prompt-handler initializes
    // it.
    this.#promptTypeToHandlerMap = null;
  }

  get activePromptHandlers() {
    return this.#promptTypeToHandlerMap;
  }

  get PromptHandlers() {
    return PromptHandlers;
  }

  get PromptTypes() {
    return PromptTypes;
  }

  /**
   * Unmarshal a JSON object representation of the unhandledPromptBehavior capability.
   *
   * @param {Record<string, ?>} json
   *     JSON Object to unmarshal.
   *
   * @throws {InvalidArgumentError}
   *     When the value of the unhandledPromptBehavior capability is invalid.
   *
   * @returns {UserPromptHandler}
   *
   * @see https://w3c.github.io/webdriver/#dfn-deserialize-as-an-unhandled-prompt-behavior
   */
  static fromJSON(json) {
    let isStringValue = false;
    let value = json;
    if (typeof value === "string") {
      // A single specified prompt behavior or for WebDriver classic.
      value = { [FALLBACK_DEFAULT_PROMPT_TYPE]: value };
      isStringValue = true;
    }

    lazy.assert.object(
      value,
      lazy.pprint`Expected "unhandledPromptBehavior" to be an object, got ${value}`
    );

    const userPromptHandlerCapability = new Map();
    for (let [promptType, handler] of Object.entries(value)) {
      if (!isStringValue) {
        const validPromptTypes = Object.values(PromptTypes);
        lazy.assert.in(
          promptType,
          validPromptTypes,
          lazy.pprint`Expected "promptType" to be one of ${validPromptTypes}, got ${promptType}`
        );
      }

      const knownPromptHandlers = Object.values(PromptHandlers);
      lazy.assert.in(
        handler,
        knownPromptHandlers,
        lazy.pprint`Expected "handler" to be one of ${knownPromptHandlers}, got ${handler}`
      );

      let notify = false;
      switch (handler) {
        case PromptHandlers.AcceptAndNotify:
          handler = PromptHandlers.Accept;
          notify = true;
          break;
        case PromptHandlers.DismissAndNotify:
          handler = PromptHandlers.Dismiss;
          notify = true;
          break;
        case PromptHandlers.Ignore:
          notify = true;
          break;
      }

      const configuration = new PromptHandlerConfiguration(handler, notify);
      userPromptHandlerCapability.set(promptType, configuration);
    }
    const userPromptHandler = new UserPromptHandler();
    userPromptHandler.update(userPromptHandlerCapability);
    return userPromptHandler;
  }

  /**
   * Get the handler for the given prompt type.
   *
   * @param {string} promptType
   *     The prompt type to retrieve the handler for.
   *
   * @returns {PromptHandlerConfiguration}
   *
   * @see https://w3c.github.io/webdriver/#dfn-get-the-prompt-handler
   */
  getPromptHandler(promptType) {
    let handlers;

    if (this.#promptTypeToHandlerMap === null) {
      handlers = new Map();
    } else {
      handlers = this.#promptTypeToHandlerMap;
    }

    if (handlers.has(promptType)) {
      return handlers.get(promptType);
    }

    if (handlers.has(PromptTypes.Default)) {
      return handlers.get(PromptTypes.Default);
    }

    if (promptType === PromptTypes.BeforeUnload) {
      return new PromptHandlerConfiguration(PromptHandlers.Accept, false);
    }

    if (handlers.has(FALLBACK_DEFAULT_PROMPT_TYPE)) {
      return handlers.get(FALLBACK_DEFAULT_PROMPT_TYPE);
    }

    return new PromptHandlerConfiguration(PromptHandlers.Dismiss, true);
  }

  /**
   * Updates the prompt handler configuration for a given requested prompt
   * handler map.
   *
   * @param {Map} requestedPromptHandler
   *     The request prompt handler configuration map.
   *
   * @see https://w3c.github.io/webdriver/#dfn-update-the-user-prompt-handler
   */
  update(requestedPromptHandler) {
    if (this.#promptTypeToHandlerMap === null) {
      this.#promptTypeToHandlerMap = new Map();
    }

    for (const [promptType, handler] of requestedPromptHandler) {
      this.#promptTypeToHandlerMap.set(promptType, handler);
    }
  }

  /**
   * JSON serialization of the user prompt handler object.
   *
   * @returns {Record<string, ?>} json
   *
   * @see https://w3c.github.io/webdriver/#dfn-serialize-the-user-prompt-handler
   */
  toJSON() {
    if (this.#promptTypeToHandlerMap === null) {
      // Fallback to "dismiss and notify" if no handler is set
      return PromptHandlers.DismissAndNotify;
    }

    if (
      this.#promptTypeToHandlerMap.size === 1 &&
      this.#promptTypeToHandlerMap.has(FALLBACK_DEFAULT_PROMPT_TYPE)
    ) {
      return this.#promptTypeToHandlerMap
        .get(FALLBACK_DEFAULT_PROMPT_TYPE)
        .toJSON();
    }

    const serialized = {};
    for (const [key, value] of this.#promptTypeToHandlerMap.entries()) {
      serialized[key] = value.toJSON();
    }

    return serialized;
  }

  toString() {
    return "[object UserPromptHandler]";
  }
}
