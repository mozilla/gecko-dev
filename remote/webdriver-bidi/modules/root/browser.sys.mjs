/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { RootBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/RootBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  Certificates: "chrome://remote/content/shared/webdriver/Certificates.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  getWebDriverSessionById:
    "chrome://remote/content/shared/webdriver/Session.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  Proxy: "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
  ProxyPerUserContextManager:
    "chrome://remote/content/webdriver-bidi/ProxyPerUserContextManager.sys.mjs",
  ProxyTypes: "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
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
  #proxyManager;
  #userContextsWithInsecureCertificatesOverrides;

  constructor(messageHandler) {
    super(messageHandler);

    this.#proxyManager = new lazy.ProxyPerUserContextManager();

    // A set of internal user context ids to keep track of user contexts
    // which had insecure certificates overrides set for them.
    this.#userContextsWithInsecureCertificatesOverrides = new Set();
  }

  destroy() {
    // Reset "allowInsecureCerts" for the userContexts,
    // which were created in the scope of this session.
    for (const userContext of this
      .#userContextsWithInsecureCertificatesOverrides) {
      lazy.Certificates.resetSecurityChecksForUserContext(userContext);
    }

    this.#userContextsWithInsecureCertificatesOverrides = null;

    this.#proxyManager.destroy();
  }

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
   * @param {object=} options
   * @param {boolean=} options.acceptInsecureCerts
   *     Indicates whether untrusted and self-signed TLS certificates
   *     should be implicitly trusted on navigation for this user context.
   * @param {object=} options.proxy
   *     An object which holds the proxy settings.
   *
   * @returns {UserContextInfo}
   *     UserContextInfo object for the created user context.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {UnsupportedOperationError}
   *     Raised when the command is called with unsupported proxy types.
   */
  async createUserContext(options = {}) {
    const { acceptInsecureCerts = null, proxy = null } = options;

    if (acceptInsecureCerts !== null) {
      lazy.assert.boolean(
        acceptInsecureCerts,
        lazy.pprint`Expected "acceptInsecureCerts" to be a boolean, got ${acceptInsecureCerts}`
      );
    }

    let proxyObject;
    if (proxy !== null) {
      proxyObject = lazy.Proxy.fromJSON(proxy);

      if (
        proxyObject.proxyType === lazy.ProxyTypes.System ||
        proxyObject.proxyType === lazy.ProxyTypes.Autodetect ||
        proxyObject.proxyType === lazy.ProxyTypes.Pac
      ) {
        // Bug 1968887: Add support for "system", "autodetect" and "pac" proxy types.
        throw new lazy.error.UnsupportedOperationError(
          `Proxy type "${proxyObject.proxyType}" is not supported`
        );
      }
    }

    const userContextId = lazy.UserContextManager.createContext("webdriver");
    const internalId = lazy.UserContextManager.getInternalIdById(userContextId);

    if (acceptInsecureCerts !== null) {
      this.#userContextsWithInsecureCertificatesOverrides.add(internalId);
      if (acceptInsecureCerts) {
        lazy.Certificates.disableSecurityChecks(internalId);
      } else {
        lazy.Certificates.enableSecurityChecks(internalId);
      }
    }

    if (proxy !== null) {
      this.#proxyManager.addConfiguration(internalId, proxyObject);
    }

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

    const internalId = lazy.UserContextManager.getInternalIdById(userContextId);

    lazy.UserContextManager.removeUserContext(userContextId, {
      closeContextTabs: true,
    });

    // Reset the state to clean up the platform state.
    lazy.Certificates.resetSecurityChecksForUserContext(internalId);
    this.#userContextsWithInsecureCertificatesOverrides.delete(internalId);

    this.#proxyManager.deleteConfiguration(internalId);
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
