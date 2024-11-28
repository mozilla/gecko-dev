/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { RootBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/RootBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  getWebDriverSessionById:
    "chrome://remote/content/shared/webdriver/Session.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  UserContextManager:
    "chrome://remote/content/shared/UserContextManager.sys.mjs",
  windowManager: "chrome://remote/content/shared/WindowManager.sys.mjs",
  WindowState: "chrome://remote/content/shared/WindowManager.sys.mjs",
});

/**
 * An object that holds information about the client window
 *
 * @typedef ClientWindowInfo
 *
 * @property {boolean} active
 *    True if client window is keyboard-interactable. False, if
 *    otherwise.
 * @property {string} clientWindow
 *    The id of the client window.
 * @property {number} height
 *    The height of the client window.
 *  @property {WindowState} state
 *    The client window state.
 * @property {number} width
 *    The width of the client window.
 * @property {number} x
 *    The x-coordinate of the client window.
 * @property {number} y
 *    The y-coordinate of the client window.
 */

/**
 * Return value of the getClientWindows command.
 *
 * @typedef GetClientWindowsResult
 *
 * @property {Array<ClientWindowInfo>} clientWindows
 */

/**
 * An object that holds information about a user context.
 *
 * @typedef UserContextInfo
 *
 * @property {string} userContext
 *     The id of the user context.
 */

/**
 * Return value for the getUserContexts command.
 *
 * @typedef GetUserContextsResult
 *
 * @property {Array<UserContextInfo>} userContexts
 *     Array of UserContextInfo for the current user contexts.
 */

class BrowserModule extends RootBiDiModule {
  constructor(messageHandler) {
    super(messageHandler);
  }

  destroy() {}

  /**
   * Commands
   */

  /**
   * Terminate all WebDriver sessions and clean up automation state in the
   * remote browser instance.
   *
   * The actual session clean-up and closing the browser will happen later
   * in WebDriverBiDiConnection class.
   */
  async close() {
    const session = lazy.getWebDriverSessionById(this.messageHandler.sessionId);

    // TODO Bug 1838269. Enable browser.close command for the case of classic + bidi session, when
    // session ending for this type of session is supported.
    if (session.http) {
      throw new lazy.error.UnsupportedOperationError(
        "Closing the browser in a session started with WebDriver classic" +
          ' is not supported. Use the WebDriver classic "Delete Session"' +
          " command instead which will also close the browser."
      );
    }

    // Close all open top-level browsing contexts by not prompting for beforeunload.
    for (const tab of lazy.TabManager.tabs) {
      lazy.TabManager.removeTab(tab, { skipPermitUnload: true });
    }
  }

  /**
   * Returns a list of client windows info
   *
   * @returns {GetClientWindowsResult}
   *     The list of client windows info
   */
  async getClientWindows() {
    const clientWindowsIds = new Set();
    const clientWindows = [];

    for (const win of lazy.windowManager.windows) {
      let clientWindowId = lazy.windowManager.getIdForWindow(win);
      if (clientWindowsIds.has(clientWindowId)) {
        continue;
      }
      clientWindowsIds.add(clientWindowId);
      let clientWindowInfo = this.#getClientWindowInfo(win);
      clientWindows.push(clientWindowInfo);
    }

    return { clientWindows };
  }

  /**
   * Creates a user context.
   *
   * @returns {UserContextInfo}
   *     UserContextInfo object for the created user context.
   */
  async createUserContext() {
    const userContextId = lazy.UserContextManager.createContext("webdriver");
    return { userContext: userContextId };
  }

  /**
   * Returns the list of available user contexts.
   *
   * @returns {GetUserContextsResult}
   *     Object containing an array of UserContextInfo.
   */
  async getUserContexts() {
    const userContexts = lazy.UserContextManager.getUserContextIds().map(
      userContextId => ({
        userContext: userContextId,
      })
    );

    return { userContexts };
  }

  /**
   * Closes a user context and all browsing contexts in it without running
   * beforeunload handlers.
   *
   * @param {object=} options
   * @param {string} options.userContext
   *     Id of the user context to close.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchUserContextError}
   *     Raised if the user context id could not be found.
   */
  async removeUserContext(options = {}) {
    const { userContext: userContextId } = options;

    lazy.assert.string(
      userContextId,
      lazy.pprint`Expected "userContext" to be a string, got ${userContextId}`
    );

    if (userContextId === lazy.UserContextManager.defaultUserContextId) {
      throw new lazy.error.InvalidArgumentError(
        `Default user context cannot be removed`
      );
    }

    if (!lazy.UserContextManager.hasUserContextId(userContextId)) {
      throw new lazy.error.NoSuchUserContextError(
        `User Context with id ${userContextId} was not found`
      );
    }
    lazy.UserContextManager.removeUserContext(userContextId, {
      closeContextTabs: true,
    });
  }

  #getClientWindowInfo(window) {
    return {
      active: Services.focus.activeWindow === window,
      clientWindow: lazy.windowManager.getIdForWindow(window),
      height: window.outerHeight,
      state: lazy.WindowState.from(window.windowState),
      width: window.outerWidth,
      x: window.screenX,
      y: window.screenY,
    };
  }
}

// To export the class as lower-case
export const browser = BrowserModule;
