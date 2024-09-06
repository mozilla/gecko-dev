/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { RootBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/RootBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  action: "chrome://remote/content/shared/webdriver/Actions.sys.mjs",
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

class InputModule extends RootBiDiModule {
  #actionsOptions;
  #inputStates;

  constructor(messageHandler) {
    super(messageHandler);

    // Browsing context => input state.
    // Bug 1821460: Move to WebDriver Session and share with Marionette.
    this.#inputStates = new WeakMap();

    // Options for actions to pass through performActions and releaseActions.
    this.#actionsOptions = {
      // Callbacks as defined in the WebDriver specification.
      getElementOrigin: this.#getElementOrigin.bind(this),
      isElementOrigin: this.#isElementOrigin.bind(this),

      // Custom callbacks.
      assertInViewPort: this.#assertInViewPort.bind(this),
      dispatchEvent: this.#dispatchEvent.bind(this),
      getClientRects: this.#getClientRects.bind(this),
      getInViewCentrePoint: this.#getInViewCentrePoint.bind(this),
    };
  }

  destroy() {}

  /**
   * Assert that the target coordinates are within the visible viewport.
   *
   * @param {Array.<number>} target
   *     Coordinates [x, y] of the target relative to the viewport.
   * @param {BrowsingContext} context
   *     The browsing context to dispatch the event to.
   *
   * @returns {Promise<undefined>}
   *     Promise that rejects, if the coordinates are not within
   *     the visible viewport.
   *
   * @throws {MoveTargetOutOfBoundsError}
   *     If target is outside the viewport.
   */
  #assertInViewPort(target, context) {
    return this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "_assertInViewPort",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { target },
    });
  }

  /**
   * Dispatch an event.
   *
   * @param {string} eventName
   *     Name of the event to be dispatched.
   * @param {BrowsingContext} context
   *     The browsing context to dispatch the event to.
   * @param {object} details
   *     Details of the event to be dispatched.
   *
   * @returns {Promise}
   *     Promise that resolves once the event is dispatched.
   */
  #dispatchEvent(eventName, context, details) {
    return this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "_dispatchEvent",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { eventName, details },
    });
  }

  /**
   * Terminates the current wheel transaction.
   *
   * @param {BrowsingContext} context
   *     The browsing context to terminate the wheel transaction for.
   *
   * @returns {Promise}
   *     Promise that resolves when the transaction was terminated.
   */
  #endWheelTransaction(context) {
    return this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "_endWheelTransaction",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
    });
  }

  /**
   * Retrieve the list of client rects for the element.
   *
   * @param {Node} node
   *     The web element reference to retrieve the rects from.
   * @param {BrowsingContext} context
   *     The browsing context to dispatch the event to.
   *
   * @returns {Promise<Array<Map.<string, number>>>}
   *     Promise that resolves to a list of DOMRect-like objects.
   */
  #getClientRects(node, context) {
    return this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "_getClientRects",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { element: node },
    });
  }

  /**
   * Retrieves the Node reference of the origin.
   *
   * @param {ElementOrigin} origin
   *     Reference to the element origin of the action.
   * @param {BrowsingContext} context
   *     The browsing context to dispatch the event to.
   *
   * @returns {Promise<SharedReference>}
   *     Promise that resolves to the shared reference.
   */
  #getElementOrigin(origin, context) {
    return this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "_getElementOrigin",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { origin },
    });
  }

  /**
   * Retrieves the action's input state.
   *
   * @param {BrowsingContext} context
   *     The Browsing Context to retrieve the input state for.
   *
   * @returns {Actions.InputState}
   *     The action's input state.
   */
  #getInputState(context) {
    // Bug 1821460: Fetch top-level browsing context.
    let inputState = this.#inputStates.get(context);

    if (inputState === undefined) {
      inputState = new lazy.action.State();
      this.#inputStates.set(context, inputState);
    }

    return inputState;
  }

  /**
   * Retrieve the in-view center point for the rect and visible viewport.
   *
   * @param {DOMRect} rect
   *     Size and position of the rectangle to check.
   * @param {BrowsingContext} context
   *     The browsing context to dispatch the event to.
   *
   * @returns {Promise<Map.<string, number>>}
   *     X and Y coordinates that denotes the in-view centre point of
   *     `rect`.
   */
  #getInViewCentrePoint(rect, context) {
    return this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "_getInViewCentrePoint",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { rect },
    });
  }

  /**
   * Checks if the given object is a valid element origin.
   *
   * @param {object} origin
   *     The object to check.
   *
   * @returns {boolean}
   *     True, if the object references a shared reference.
   */
  #isElementOrigin(origin) {
    return (
      origin?.type === "element" && typeof origin.element?.sharedId === "string"
    );
  }

  /**
   * Resets the action's input state.
   *
   * @param {BrowsingContext} context
   *     The Browsing Context to reset the input state for.
   */
  #resetInputState(context) {
    // Bug 1821460: Fetch top-level browsing context.
    if (this.#inputStates.has(context)) {
      this.#inputStates.delete(context);
    }
  }

  async performActions(options = {}) {
    const { actions, context: contextId } = options;

    lazy.assert.string(
      contextId,
      lazy.pprint`Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }

    const inputState = this.#getInputState(context);
    const actionsOptions = { ...this.#actionsOptions, context };

    const actionChain = await lazy.action.Chain.fromJSON(
      inputState,
      actions,
      actionsOptions
    );

    // Enqueue to serialize access to input state.
    await inputState.enqueueAction(() =>
      actionChain.dispatch(inputState, actionsOptions)
    );

    await this.#endWheelTransaction(context);
  }

  /**
   * Reset the input state in the provided browsing context.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to reset the input state.
   *
   * @throws {InvalidArgumentError}
   *     If <var>context</var> is not valid type.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  async releaseActions(options = {}) {
    const { context: contextId } = options;

    lazy.assert.string(
      contextId,
      lazy.pprint`Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }

    const inputState = this.#getInputState(context);
    const actionsOptions = { ...this.#actionsOptions, context };

    // Enqueue to serialize access to input state.
    await inputState.enqueueAction(() => {
      const undoActions = inputState.inputCancelList.reverse();
      return undoActions.dispatch(inputState, actionsOptions);
    });

    this.#resetInputState(context);
  }

  /**
   * Sets the file property of a given input element with type file to a set of file paths.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to set the file property
   *     of a given input element.
   * @param {SharedReference} options.element
   *     A reference to a node, which is used as
   *     a target for setting files.
   * @param {Array<string>} options.files
   *     A list of file paths which should be set.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchElementError}
   *     If the input element cannot be found.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   * @throws {UnableToSetFileInputError}
   *     If the set of file paths was not set to the input element.
   */
  async setFiles(options = {}) {
    const { context: contextId, element, files } = options;

    lazy.assert.string(
      contextId,
      lazy.pprint`Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }

    lazy.assert.array(
      files,
      lazy.pprint`Expected "files" to be an array, got ${files}`
    );

    for (const file of files) {
      lazy.assert.string(
        file,
        lazy.pprint`Expected an element of "files" to be a string, got ${file}`
      );
    }

    await this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "setFiles",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { element, files },
    });
  }

  static get supportedEvents() {
    return [];
  }
}

export const input = InputModule;
