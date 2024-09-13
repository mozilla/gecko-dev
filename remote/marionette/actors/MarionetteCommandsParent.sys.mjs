/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  action: "chrome://remote/content/shared/webdriver/Actions.sys.mjs",
  capture: "chrome://remote/content/shared/Capture.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  getSeenNodesForBrowsingContext:
    "chrome://remote/content/shared/webdriver/Session.sys.mjs",
  json: "chrome://remote/content/marionette/json.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  WebElement: "chrome://remote/content/marionette/web-reference.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

// Because Marionette supports a single session only we store its id
// globally so that the parent actor can access it.
let webDriverSessionId = null;

export class MarionetteCommandsParent extends JSWindowActorParent {
  #actionsOptions;
  #actionState;
  #deferredDialogOpened;

  actorCreated() {
    // The {@link Actions.State} of the input actions.
    this.#actionState = null;

    // Options for actions to pass through performActions and releaseActions.
    this.#actionsOptions = {
      // Callbacks as defined in the WebDriver specification.
      getElementOrigin: this.#getElementOrigin.bind(this),
      isElementOrigin: this.#isElementOrigin.bind(this),

      // Custom properties and callbacks
      context: this.browsingContext,

      assertInViewPort: this.#assertInViewPort.bind(this),
      dispatchEvent: this.#dispatchEvent.bind(this),
      getClientRects: this.#getClientRects.bind(this),
      getInViewCentrePoint: this.#getInViewCentrePoint.bind(this),
    };

    this.#deferredDialogOpened = null;
  }

  /**
   * Assert that the target coordinates are within the visible viewport.
   *
   * @param {Array.<number>} target
   *     Coordinates [x, y] of the target relative to the viewport.
   * @param {BrowsingContext} _context
   *     Unused in Marionette.
   *
   * @returns {Promise<undefined>}
   *     Promise that rejects, if the coordinates are not within
   *     the visible viewport.
   *
   * @throws {MoveTargetOutOfBoundsError}
   *     If target is outside the viewport.
   */
  #assertInViewPort(target, _context) {
    return this.sendQuery("MarionetteCommandsParent:_assertInViewPort", {
      target,
    });
  }

  /**
   * Dispatch an event.
   *
   * @param {string} eventName
   *     Name of the event to be dispatched.
   * @param {BrowsingContext} _context
   *     Unused in Marionette.
   * @param {object} details
   *     Details of the event to be dispatched.
   *
   * @returns {Promise}
   *     Promise that resolves once the event is dispatched.
   */
  #dispatchEvent(eventName, _context, details) {
    return this.sendQuery("MarionetteCommandsParent:_dispatchEvent", {
      eventName,
      details,
    });
  }

  /**
   * Finalize an action command.
   *
   * @returns {Promise}
   *     Promise that resolves when the finalization is done.
   */
  #finalizeAction() {
    return this.sendQuery("MarionetteCommandsParent:_finalizeAction");
  }

  /**
   * Retrieves the WebElement reference of the origin.
   *
   * @param {ElementOrigin} origin
   *     Reference to the element origin of the action.
   * @param {BrowsingContext} _context
   *     Unused in Marionette.
   *
   * @returns {WebElement}
   *     The WebElement reference.
   */
  #getElementOrigin(origin, _context) {
    return origin;
  }

  /**
   * Retrieve the list of client rects for the element.
   *
   * @param {WebElement} element
   *     The web element reference to retrieve the rects from.
   * @param {BrowsingContext} _context
   *     Unused in Marionette.
   *
   * @returns {Promise<Array<Map.<string, number>>>}
   *     Promise that resolves to a list of DOMRect-like objects.
   */
  #getClientRects(element, _context) {
    return this.executeScript("return arguments[0].getClientRects()", [
      element,
    ]);
  }

  /**
   * Retrieve the in-view center point for the rect and visible viewport.
   *
   * @param {DOMRect} rect
   *     Size and position of the rectangle to check.
   * @param { BrowsingContext } _context
   *     Unused in Marionette.
   *
   * @returns {Promise<Map.<string, number>>}
   *     X and Y coordinates that denotes the in-view centre point of
   *     `rect`.
   */
  #getInViewCentrePoint(rect, _context) {
    return this.sendQuery("MarionetteCommandsParent:_getInViewCentrePoint", {
      rect,
    });
  }

  /**
   * Checks if the given object is a valid element origin.
   *
   * @param {object} origin
   *     The object to check.
   *
   * @returns {boolean}
   *     True, if it's a WebElement.
   */
  #isElementOrigin(origin) {
    return lazy.WebElement.Identifier in origin;
  }

  async sendQuery(name, serializedValue) {
    const seenNodes = lazy.getSeenNodesForBrowsingContext(
      webDriverSessionId,
      this.manager.browsingContext
    );

    // return early if a dialog is opened
    this.#deferredDialogOpened = Promise.withResolvers();
    let {
      error,
      seenNodeIds,
      serializedValue: serializedResult,
      hasSerializedWindows,
    } = await Promise.race([
      super.sendQuery(name, serializedValue),
      this.#deferredDialogOpened.promise,
    ]).finally(() => {
      this.#deferredDialogOpened = null;
    });

    if (error) {
      const err = lazy.error.WebDriverError.fromJSON(error);
      this.#handleError(err, seenNodes);
    }

    // Update seen nodes for serialized element and shadow root nodes.
    seenNodeIds?.forEach(nodeId => seenNodes.add(nodeId));

    if (hasSerializedWindows) {
      // The serialized data contains WebWindow references that need to be
      // converted to unique identifiers.
      serializedResult = lazy.json.mapToNavigableIds(serializedResult);
    }

    return serializedResult;
  }

  /**
   * Handle WebDriver error and replace error type if necessary.
   *
   * @param {WebDriverError} error
   *     The WebDriver error to handle.
   * @param {Set<string>} seenNodes
   *     List of node ids already seen in this navigable.
   *
   * @throws {WebDriverError}
   *     The original or replaced WebDriver error.
   */
  #handleError(error, seenNodes) {
    // If an element hasn't been found during deserialization check if it
    // may be a stale reference.
    if (
      error instanceof lazy.error.NoSuchElementError &&
      error.data.elementId !== undefined &&
      seenNodes.has(error.data.elementId)
    ) {
      throw new lazy.error.StaleElementReferenceError(error);
    }

    // If a shadow root hasn't been found during deserialization check if it
    // may be a detached reference.
    if (
      error instanceof lazy.error.NoSuchShadowRootError &&
      error.data.shadowId !== undefined &&
      seenNodes.has(error.data.shadowId)
    ) {
      throw new lazy.error.DetachedShadowRootError(error);
    }

    throw error;
  }

  notifyDialogOpened() {
    if (this.#deferredDialogOpened) {
      this.#deferredDialogOpened.resolve({ data: null });
    }
  }

  // Proxying methods for WebDriver commands

  clearElement(webEl) {
    return this.sendQuery("MarionetteCommandsParent:clearElement", {
      elem: webEl,
    });
  }

  clickElement(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:clickElement", {
      elem: webEl,
      capabilities: capabilities.toJSON(),
    });
  }

  async executeScript(script, args, opts) {
    return this.sendQuery("MarionetteCommandsParent:executeScript", {
      script,
      args: lazy.json.mapFromNavigableIds(args),
      opts,
    });
  }

  findElement(strategy, selector, opts) {
    return this.sendQuery("MarionetteCommandsParent:findElement", {
      strategy,
      selector,
      opts,
    });
  }

  findElements(strategy, selector, opts) {
    return this.sendQuery("MarionetteCommandsParent:findElements", {
      strategy,
      selector,
      opts,
    });
  }

  async getShadowRoot(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getShadowRoot", {
      elem: webEl,
    });
  }

  async getActiveElement() {
    return this.sendQuery("MarionetteCommandsParent:getActiveElement");
  }

  async getComputedLabel(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getComputedLabel", {
      elem: webEl,
    });
  }

  async getComputedRole(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getComputedRole", {
      elem: webEl,
    });
  }

  async getElementAttribute(webEl, name) {
    return this.sendQuery("MarionetteCommandsParent:getElementAttribute", {
      elem: webEl,
      name,
    });
  }

  async getElementProperty(webEl, name) {
    return this.sendQuery("MarionetteCommandsParent:getElementProperty", {
      elem: webEl,
      name,
    });
  }

  async getElementRect(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getElementRect", {
      elem: webEl,
    });
  }

  async getElementTagName(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getElementTagName", {
      elem: webEl,
    });
  }

  async getElementText(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getElementText", {
      elem: webEl,
    });
  }

  async getElementValueOfCssProperty(webEl, name) {
    return this.sendQuery(
      "MarionetteCommandsParent:getElementValueOfCssProperty",
      {
        elem: webEl,
        name,
      }
    );
  }

  async getPageSource() {
    return this.sendQuery("MarionetteCommandsParent:getPageSource");
  }

  async isElementDisplayed(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:isElementDisplayed", {
      capabilities: capabilities.toJSON(),
      elem: webEl,
    });
  }

  async isElementEnabled(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:isElementEnabled", {
      capabilities: capabilities.toJSON(),
      elem: webEl,
    });
  }

  async isElementSelected(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:isElementSelected", {
      capabilities: capabilities.toJSON(),
      elem: webEl,
    });
  }

  async sendKeysToElement(webEl, text, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:sendKeysToElement", {
      capabilities: capabilities.toJSON(),
      elem: webEl,
      text,
    });
  }

  async performActions(actions) {
    // Bug 1821460: Use top-level browsing context.
    if (this.#actionState === null) {
      this.#actionState = new lazy.action.State();
    }

    const actionChain = await lazy.action.Chain.fromJSON(
      this.#actionState,
      actions,
      this.#actionsOptions
    );

    // Enqueue to serialize access to input state.
    await this.#actionState.enqueueAction(() =>
      actionChain.dispatch(this.#actionState, this.#actionsOptions)
    );

    // Process async follow-up tasks in content before the reply is sent.
    await this.#finalizeAction();
  }

  /**
   * The release actions command is used to release all the keys and pointer
   * buttons that are currently depressed. This causes events to be fired
   * as if the state was released by an explicit series of actions. It also
   * clears all the internal state of the virtual devices.
   */
  async releaseActions() {
    // Bug 1821460: Use top-level browsing context.
    if (this.#actionState === null) {
      return;
    }

    // Enqueue to serialize access to input state.
    await this.#actionState.enqueueAction(() => {
      const undoActions = this.#actionState.inputCancelList.reverse();
      undoActions.dispatch(this.#actionState, this.#actionsOptions);
    });

    this.#actionState = null;

    // Process async follow-up tasks in content before the reply is sent.
    await this.#finalizeAction();
  }

  async switchToFrame(id) {
    const { browsingContextId } = await this.sendQuery(
      "MarionetteCommandsParent:switchToFrame",
      { id }
    );

    return {
      browsingContext: BrowsingContext.get(browsingContextId),
    };
  }

  async switchToParentFrame() {
    const { browsingContextId } = await this.sendQuery(
      "MarionetteCommandsParent:switchToParentFrame"
    );

    return {
      browsingContext: BrowsingContext.get(browsingContextId),
    };
  }

  async takeScreenshot(webEl, format, full, scroll) {
    const rect = await this.sendQuery(
      "MarionetteCommandsParent:getScreenshotRect",
      {
        elem: webEl,
        full,
        scroll,
      }
    );

    // If no element has been specified use the top-level browsing context.
    // Otherwise use the browsing context from the currently selected frame.
    const browsingContext = webEl
      ? this.browsingContext
      : this.browsingContext.top;

    let canvas = await lazy.capture.canvas(
      browsingContext.topChromeWindow,
      browsingContext,
      rect.x,
      rect.y,
      rect.width,
      rect.height
    );

    switch (format) {
      case lazy.capture.Format.Hash:
        return lazy.capture.toHash(canvas);

      case lazy.capture.Format.Base64:
        return lazy.capture.toBase64(canvas);

      default:
        throw new TypeError(`Invalid capture format: ${format}`);
    }
  }
}

/**
 * Proxy that will dynamically create MarionetteCommands actors for a dynamically
 * provided browsing context until the method can be fully executed by the
 * JSWindowActor pair.
 *
 * @param {function(): BrowsingContext} browsingContextFn
 *     A function that returns the reference to the browsing context for which
 *     the query should run.
 */
export function getMarionetteCommandsActorProxy(browsingContextFn) {
  const MAX_ATTEMPTS = 10;

  /**
   * Methods which modify the content page cannot be retried safely.
   * See Bug 1673345.
   */
  const NO_RETRY_METHODS = [
    "clickElement",
    "executeScript",
    "performActions",
    "releaseActions",
    "sendKeysToElement",
  ];

  return new Proxy(
    {},
    {
      get(target, methodName) {
        return async (...args) => {
          let attempts = 0;
          while (true) {
            try {
              const browsingContext = browsingContextFn();
              if (!browsingContext) {
                throw new DOMException(
                  "No BrowsingContext found",
                  "NoBrowsingContext"
                );
              }

              // TODO: Scenarios where the window/tab got closed and
              // currentWindowGlobal is null will be handled in Bug 1662808.
              const actor =
                browsingContext.currentWindowGlobal.getActor(
                  "MarionetteCommands"
                );

              const result = await actor[methodName](...args);
              return result;
            } catch (e) {
              if (!["AbortError", "InactiveActor"].includes(e.name)) {
                // Only retry when the JSWindowActor pair gets destroyed, or
                // gets inactive eg. when the page is moved into bfcache.
                throw e;
              }

              if (NO_RETRY_METHODS.includes(methodName)) {
                const browsingContextId = browsingContextFn()?.id;
                lazy.logger.trace(
                  `[${browsingContextId}] Querying "${methodName}" failed with` +
                    ` ${e.name}, returning "null" as fallback`
                );
                return null;
              }

              if (++attempts > MAX_ATTEMPTS) {
                const browsingContextId = browsingContextFn()?.id;
                lazy.logger.trace(
                  `[${browsingContextId}] Querying "${methodName} "` +
                    `reached the limit of retry attempts (${MAX_ATTEMPTS})`
                );
                throw e;
              }

              lazy.logger.trace(
                `Retrying "${methodName}", attempt: ${attempts}`
              );
            }
          }
        };
      },
    }
  );
}

/**
 * Register the MarionetteCommands actor that holds all the commands.
 *
 * @param {string} sessionId
 *     The id of the current WebDriver session.
 */
export function registerCommandsActor(sessionId) {
  try {
    ChromeUtils.registerWindowActor("MarionetteCommands", {
      kind: "JSWindowActor",
      parent: {
        esModuleURI:
          "chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs",
      },
      child: {
        esModuleURI:
          "chrome://remote/content/marionette/actors/MarionetteCommandsChild.sys.mjs",
      },

      allFrames: true,
      includeChrome: true,
    });
  } catch (e) {
    if (e.name === "NotSupportedError") {
      lazy.logger.warn(`MarionetteCommands actor is already registered!`);
    } else {
      throw e;
    }
  }

  webDriverSessionId = sessionId;
}

export function unregisterCommandsActor() {
  webDriverSessionId = null;

  ChromeUtils.unregisterWindowActor("MarionetteCommands");
}
