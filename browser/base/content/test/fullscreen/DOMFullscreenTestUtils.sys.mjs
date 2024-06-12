/* global content */
const testContext = {
  scope: null,
  windowGlobal: null,
};

export var DOMFullscreenTestUtils = {
  /**
   * Running this init allows helpers to access test scope helpers, like Assert
   * and SimpleTest.
   * Tests should call init() before using the helpers which rely on properties assigned here.
   *
   * @param {object} scope The global scope where tests are being run.
   * @param {Window} win The DOM Window global
   */
  init(scope, win) {
    if (!scope) {
      throw new Error(
        "Must initialize DOMFullscreenTestUtils with a test scope"
      );
    }
    if (!win) {
      throw new Error(
        "Must initialize DOMFullscreenTestUtils with a windowGlobal"
      );
    }
    testContext.scope = scope;
    testContext.windowGlobal = win;
    testContext.scope.registerCleanupFunction(() => {
      delete testContext.scope;
      delete testContext.windowGlobal;
    });
  },

  waitForFullScreenState(browser, state, actionAfterFSEvent) {
    return new Promise(resolve => {
      let eventReceived = false;

      let observe = () => {
        if (!eventReceived) {
          return;
        }
        Services.obs.removeObserver(observe, "fullscreen-painted");
        resolve();
      };
      Services.obs.addObserver(observe, "fullscreen-painted");

      browser.ownerGlobal.addEventListener(
        `MozDOMFullscreen:${state ? "Entered" : "Exited"}`,
        () => {
          eventReceived = true;
          if (actionAfterFSEvent) {
            actionAfterFSEvent();
          }
        },
        { once: true }
      );
    });
  },

  /**
   * Spawns content task in browser to enter / leave fullscreen
   * @param browser - Browser to use for JS fullscreen requests
   * @param {Boolean} fullscreenState - true to enter fullscreen, false to leave
   * @returns {Promise} - Resolves once fullscreen change is applied
   */
  async changeFullscreen(browser, fullScreenState) {
    if (!testContext.scope) {
      throw new Error(
        "Must first initialize DOMFullscreenTestUtils with a test scope"
      );
    }
    await new Promise(resolve =>
      testContext.scope.SimpleTest.waitForFocus(resolve, browser.ownerGlobal)
    );
    let fullScreenChange = DOMFullscreenTestUtils.waitForFullScreenState(
      browser,
      fullScreenState
    );
    testContext.windowGlobal.SpecialPowers.spawn(
      browser,
      [fullScreenState],
      async state => {
        // Wait for document focus before requesting full-screen
        const { ContentTaskUtils } = ChromeUtils.importESModule(
          "resource://testing-common/ContentTaskUtils.sys.mjs"
        );
        await ContentTaskUtils.waitForCondition(
          () => content.browsingContext.isActive && content.document.hasFocus(),
          "Waiting for document focus"
        );
        if (state) {
          content.document.body.requestFullscreen();
        } else {
          content.document.exitFullscreen();
        }
      }
    );
    return fullScreenChange;
  },
};
