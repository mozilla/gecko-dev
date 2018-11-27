/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module implements a number of utilities useful for browser tests.
 *
 * All asynchronous helper methods should return promises, rather than being
 * callback based.
 */

// This file uses ContentTask & frame scripts, where these are available.
/* global addEventListener, removeEventListener, sendAsyncMessage,
          addMessageListener, removeMessageListener, privateNoteIntentionalCrash */

"use strict";

var EXPORTED_SYMBOLS = [
  "BrowserTestUtils",
];

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/TestUtils.jsm");
ChromeUtils.import("resource://testing-common/ContentTask.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm");

Services
  .mm
  .loadFrameScript(
    "chrome://mochikit/content/tests/BrowserTestUtils/content-utils.js", true);

ChromeUtils.defineModuleGetter(this, "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm");

const PROCESSSELECTOR_CONTRACTID = "@mozilla.org/ipc/processselector;1";
const OUR_PROCESSSELECTOR_CID =
  Components.ID("{f9746211-3d53-4465-9aeb-ca0d96de0253}");
const EXISTING_JSID = Cc[PROCESSSELECTOR_CONTRACTID];
const DEFAULT_PROCESSSELECTOR_CID = EXISTING_JSID ?
  Components.ID(EXISTING_JSID.number) : null;

let gListenerId = 0;

// A process selector that always asks for a new process.
function NewProcessSelector() {
}

NewProcessSelector.prototype = {
  classID: OUR_PROCESSSELECTOR_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentProcessProvider]),

  provideProcess() {
    return Ci.nsIContentProcessProvider.NEW_PROCESS;
  },
};

let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
let selectorFactory = XPCOMUtils._getFactory(NewProcessSelector);
registrar.registerFactory(OUR_PROCESSSELECTOR_CID, "", null, selectorFactory);

// For now, we'll allow tests to use CPOWs in this module for
// some cases.
Cu.permitCPOWsInScope(this);

var gSendCharCount = 0;
var gSynthesizeKeyCount = 0;
var gSynthesizeCompositionCount = 0;
var gSynthesizeCompositionChangeCount = 0;

const kAboutPageRegistrationContentScript =
  "chrome://mochikit/content/tests/BrowserTestUtils/content-about-page-utils.js";

var BrowserTestUtils = {
  /**
   * Loads a page in a new tab, executes a Task and closes the tab.
   *
   * @param options
   *        An object  or string.
   *        If this is a string it is the url to open and will be opened in the
   *        currently active browser window.
   *        If an object it should have the following properties:
   *        {
   *          gBrowser:
   *            Reference to the "tabbrowser" element where the new tab should
   *            be opened.
   *          url:
   *            String with the URL of the page to load.
   *        }
   * @param taskFn
   *        Generator function representing a Task that will be executed while
   *        the tab is loaded. The first argument passed to the function is a
   *        reference to the browser object for the new tab.
   *
   * @return {} Returns the value that is returned from taskFn.
   * @resolves When the tab has been closed.
   * @rejects Any exception from taskFn is propagated.
   */
  async withNewTab(options, taskFn) {
    if (typeof(options) == "string") {
      options = {
        gBrowser: Services.wm.getMostRecentWindow("navigator:browser").gBrowser,
        url: options,
      };
    }
    let tab = await BrowserTestUtils.openNewForegroundTab(options);
    let originalWindow = tab.ownerGlobal;
    let result = await taskFn(tab.linkedBrowser);
    let finalWindow = tab.ownerGlobal;
    if (originalWindow == finalWindow && !tab.closing && tab.linkedBrowser) {
      // taskFn may resolve within a tick after opening a new tab.
      // We shouldn't remove the newly opened tab in the same tick.
      // Wait for the next tick here.
      await TestUtils.waitForTick();
      BrowserTestUtils.removeTab(tab);
    } else {
      Services.console.logStringMessage(
        "BrowserTestUtils.withNewTab: Tab was already closed before " +
        "removeTab would have been called");
    }
    return Promise.resolve(result);
  },

  /**
   * Opens a new tab in the foreground.
   *
   * This function takes an options object (which is preferred) or actual
   * parameters. The names of the options must correspond to the names below.
   * gBrowser is required and all other options are optional.
   *
   * @param {tabbrowser} gBrowser
   *        The tabbrowser to open the tab new in.
   * @param {string} opening (or url)
   *        May be either a string URL to load in the tab, or a function that
   *        will be called to open a foreground tab. Defaults to "about:blank".
   * @param {boolean} waitForLoad
   *        True to wait for the page in the new tab to load. Defaults to true.
   * @param {boolean} waitForStateStop
   *        True to wait for the web progress listener to send STATE_STOP for the
   *        document in the tab. Defaults to false.
   * @param {boolean} forceNewProcess
   *        True to force the new tab to load in a new process. Defaults to
   *        false.
   *
   * @return {Promise}
   *         Resolves when the tab is ready and loaded as necessary.
   * @resolves The new tab.
   */
  openNewForegroundTab(tabbrowser, ...args) {
    let options;
    if (tabbrowser.ownerGlobal && tabbrowser === tabbrowser.ownerGlobal.gBrowser) {
      // tabbrowser is a tabbrowser, read the rest of the arguments from args.
      let [
        opening = "about:blank",
        waitForLoad = true,
        waitForStateStop = false,
        forceNewProcess = false,
      ] = args;

      options = { opening, waitForLoad, waitForStateStop, forceNewProcess };
    } else {
      if ("url" in tabbrowser && !("opening" in tabbrowser)) {
        tabbrowser.opening = tabbrowser.url;
      }

      let {
        opening = "about:blank",
        waitForLoad = true,
        waitForStateStop = false,
        forceNewProcess = false,
      } = tabbrowser;

      tabbrowser = tabbrowser.gBrowser;
      options = { opening, waitForLoad, waitForStateStop, forceNewProcess };
    }

    let { opening: opening,
          waitForLoad: aWaitForLoad,
          waitForStateStop: aWaitForStateStop,
    } = options;

    let promises, tab;
    try {
      // If we're asked to force a new process, replace the normal process
      // selector with one that always asks for a new process.
      // If DEFAULT_PROCESSSELECTOR_CID is null, we're in non-e10s mode and we
      // should skip this.
      if (options.forceNewProcess && DEFAULT_PROCESSSELECTOR_CID) {
        Services.ppmm.releaseCachedProcesses();
        registrar.registerFactory(OUR_PROCESSSELECTOR_CID, "",
                                  PROCESSSELECTOR_CONTRACTID, null);
      }

      promises = [
        BrowserTestUtils.switchTab(tabbrowser, function() {
          if (typeof opening == "function") {
            opening();
            tab = tabbrowser.selectedTab;
          } else {
            tabbrowser.selectedTab = tab = BrowserTestUtils.addTab(tabbrowser, opening);
          }
        }),
      ];

      if (aWaitForLoad) {
        promises.push(BrowserTestUtils.browserLoaded(tab.linkedBrowser));
      }
      if (aWaitForStateStop) {
        promises.push(BrowserTestUtils.browserStopped(tab.linkedBrowser));
      }
    } finally {
      // Restore the original process selector, if needed.
      if (options.forceNewProcess && DEFAULT_PROCESSSELECTOR_CID) {
        registrar.registerFactory(DEFAULT_PROCESSSELECTOR_CID, "",
                                  PROCESSSELECTOR_CONTRACTID, null);
      }
    }
    return Promise.all(promises).then(() => tab);
  },

  /**
   * Checks if a DOM element is hidden.
   *
   * @param {Element} element
   *        The element which is to be checked.
   *
   * @return {boolean}
   */
  is_hidden(element) {
    var style = element.ownerGlobal.getComputedStyle(element);
    if (style.display == "none")
      return true;
    if (style.visibility != "visible")
      return true;
    if (style.display == "-moz-popup")
      return ["hiding", "closed"].includes(element.state);

    // Hiding a parent element will hide all its children
    if (element.parentNode != element.ownerDocument)
      return BrowserTestUtils.is_hidden(element.parentNode);

    return false;
  },

  /**
   * Checks if a DOM element is visible.
   *
   * @param {Element} element
   *        The element which is to be checked.
   *
   * @return {boolean}
   */
  is_visible(element) {
    var style = element.ownerGlobal.getComputedStyle(element);
    if (style.display == "none")
      return false;
    if (style.visibility != "visible")
      return false;
    if (style.display == "-moz-popup" && element.state != "open")
      return false;

    // Hiding a parent element will hide all its children
    if (element.parentNode != element.ownerDocument)
      return BrowserTestUtils.is_visible(element.parentNode);

    return true;
  },

  /**
   * Switches to a tab and resolves when it is ready.
   *
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser.
   * @param {tab} tab
   *        Either a tab element to switch to or a function to perform the switch.
   *
   * @return {Promise}
   *         Resolves when the tab has been switched to.
   * @resolves The tab switched to.
   */
  switchTab(tabbrowser, tab) {
    let promise = new Promise(resolve => {
      tabbrowser.addEventListener("TabSwitchDone", function() {
        TestUtils.executeSoon(() => resolve(tabbrowser.selectedTab));
      }, {once: true});
    });

    if (typeof tab == "function") {
      tab();
    } else {
      tabbrowser.selectedTab = tab;
    }
    return promise;
  },

  /**
   * Waits for an ongoing page load in a browser window to complete.
   *
   * This can be used in conjunction with any synchronous method for starting a
   * load, like the "addTab" method on "tabbrowser", and must be called before
   * yielding control to the event loop. This is guaranteed to work because the
   * way we're listening for the load is in the content-utils.js frame script,
   * and then sending an async message up, so we can't miss the message.
   *
   * @param {xul:browser} browser
   *        A xul:browser.
   * @param {Boolean} includeSubFrames
   *        A boolean indicating if loads from subframes should be included.
   * @param {optional string or function} wantLoad
   *        If a function, takes a URL and returns true if that's the load we're
   *        interested in. If a string, gives the URL of the load we're interested
   *        in. If not present, the first load resolves the promise.
   * @param {optional boolean} maybeErrorPage
   *        If true, this uses DOMContentLoaded event instead of load event.
   *        Also wantLoad will be called with visible URL, instead of
   *        'about:neterror?...' for error page.
   *
   * @return {Promise}
   * @resolves When a load event is triggered for the browser.
   */
  browserLoaded(browser, includeSubFrames = false, wantLoad = null,
                maybeErrorPage = false) {
    // Passing a url as second argument is a common mistake we should prevent.
    if (includeSubFrames && typeof includeSubFrames != "boolean") {
      throw ("The second argument to browserLoaded should be a boolean.");
    }

    // If browser belongs to tabbrowser-tab, ensure it has been
    // inserted into the document.
    let tabbrowser = browser.ownerGlobal.gBrowser;
    if (tabbrowser && tabbrowser.getTabForBrowser) {
      tabbrowser._insertBrowser(tabbrowser.getTabForBrowser(browser));
    }

    function isWanted(url) {
      if (!wantLoad) {
        return true;
      } else if (typeof(wantLoad) == "function") {
        return wantLoad(url);
      }
        // It's a string.
        return wantLoad == url;

    }

    return new Promise(resolve => {
      let mm = browser.ownerGlobal.messageManager;
      let eventName = maybeErrorPage
          ? "browser-test-utils:DOMContentLoadedEvent"
          : "browser-test-utils:loadEvent";
      mm.addMessageListener(eventName, function onLoad(msg) {
        // See testing/mochitest/BrowserTestUtils/content/content-utils.js for
        // the difference between visibleURL and internalURL.
        if (msg.target == browser && (!msg.data.subframe || includeSubFrames) &&
            isWanted(maybeErrorPage
                     ? msg.data.visibleURL : msg.data.internalURL)) {
          mm.removeMessageListener(eventName, onLoad);
          resolve(msg.data.internalURL);
        }
      });
    });
  },

  /**
   * Waits for the selected browser to load in a new window. This
   * is most useful when you've got a window that might not have
   * loaded its DOM yet, and where you can't easily use browserLoaded
   * on gBrowser.selectedBrowser since gBrowser doesn't yet exist.
   *
   * @param {xul:window} window
   *        A newly opened window for which we're waiting for the
   *        first browser load.
   * @param {Boolean} aboutBlank [optional]
   *        If false, about:blank loads are ignored and we continue
   *        to wait.
   * @param {function or null} checkFn [optional]
   *        If checkFn(browser) returns false, the load is ignored
   *        and we continue to wait.
   *
   * @return {Promise}
   * @resolves Once the selected browser fires its load event.
   */
  firstBrowserLoaded(win, aboutBlank = true, checkFn = null) {
    let mm = win.messageManager;
    return this.waitForMessage(mm, "browser-test-utils:loadEvent", (msg) => {
      if (checkFn) {
        return checkFn(msg.target);
      }

      let selectedBrowser = win.gBrowser.selectedBrowser;
      return msg.target == selectedBrowser &&
             (aboutBlank || selectedBrowser.currentURI.spec != "about:blank");
    });
  },

  _webProgressListeners: new Set(),

  /**
   * Waits for the web progress listener associated with this tab to fire a
   * STATE_STOP for the toplevel document.
   *
   * @param {xul:browser} browser
   *        A xul:browser.
   * @param {String} expectedURI (optional)
   *        A specific URL to check the channel load against
   * @param {Boolean} checkAborts (optional, defaults to false)
   *        Whether NS_BINDING_ABORTED stops 'count' as 'real' stops
   *        (e.g. caused by the stop button or equivalent APIs)
   *
   * @return {Promise}
   * @resolves When STATE_STOP reaches the tab's progress listener
   */
  browserStopped(browser, expectedURI, checkAborts = false) {
    return new Promise(resolve => {
      let wpl = {
        onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
          dump("Saw state " + aStateFlags.toString(16) + " and status " + aStatus.toString(16) + "\n");
          if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
              aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
              (checkAborts || aStatus != Cr.NS_BINDING_ABORTED) &&
              aWebProgress.isTopLevel) {
            let chan = aRequest.QueryInterface(Ci.nsIChannel);
            dump("Browser loaded " + chan.originalURI.spec + "\n");
            if (!expectedURI || chan.originalURI.spec == expectedURI) {
              browser.removeProgressListener(wpl);
              BrowserTestUtils._webProgressListeners.delete(wpl);
              resolve();
            }
          }
        },
        onSecurityChange() {},
        onStatusChange() {},
        onLocationChange() {},
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIWebProgressListener,
          Ci.nsIWebProgressListener2,
          Ci.nsISupportsWeakReference,
        ]),
      };
      browser.addProgressListener(wpl);
      this._webProgressListeners.add(wpl);
      dump("Waiting for browser load" + (expectedURI ? (" of " + expectedURI) : "") + "\n");
    });
  },

  /**
   * Waits for a tab to open and load a given URL.
   *
   * The method doesn't wait for the tab contents to load.
   *
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser to look for the next new tab in.
   * @param {string} url
   *        A string URL to look for in the new tab. If null, allows any non-blank URL.
   * @param {boolean} waitForLoad
   *        True to wait for the page in the new tab to load. Defaults to false.
   * @param {boolean} waitForAnyTab
   *        True to wait for the url to be loaded in any new tab, not just the next
   *        one opened.
   *
   * @return {Promise}
   * @resolves With the {xul:tab} when a tab is opened and its location changes
   *           to the given URL and optionally that browser has loaded.
   *
   * NB: this method will not work if you open a new tab with e.g. BrowserOpenTab
   * and the tab does not load a URL, because no onLocationChange will fire.
   */
  waitForNewTab(tabbrowser, url, waitForLoad = false, waitForAnyTab = false) {
    let urlMatches = url ? (urlToMatch) => urlToMatch == url
                         : (urlToMatch) => urlToMatch != "about:blank";
    return new Promise((resolve, reject) => {
      tabbrowser.tabContainer.addEventListener("TabOpen", function tabOpenListener(openEvent) {
        if (!waitForAnyTab) {
          tabbrowser.tabContainer.removeEventListener("TabOpen", tabOpenListener);
        }
        let newTab = openEvent.target;
        let newBrowser = newTab.linkedBrowser;
        let result;
        if (waitForLoad) {
          // If waiting for load, resolve with promise for that, which when load
          // completes resolves to the new tab.
          result = BrowserTestUtils.browserLoaded(newBrowser, false, urlMatches)
                                   .then(() => newTab);
        } else {
          // If not waiting for load, just resolve with the new tab.
          result = newTab;
        }

        let progressListener = {
          onLocationChange(aBrowser) {
            // Only interested in location changes on our browser.
            if (aBrowser != newBrowser) {
              return;
            }

            // Check that new location is the URL we want.
            if (!urlMatches(aBrowser.currentURI.spec)) {
              return;
            }
            if (waitForAnyTab) {
              tabbrowser.tabContainer.removeEventListener("TabOpen", tabOpenListener);
            }
            tabbrowser.removeTabsProgressListener(progressListener);
            TestUtils.executeSoon(() => resolve(result));
          },
        };
        tabbrowser.addTabsProgressListener(progressListener);
      });
    });
  },

  /**
   * Waits for onLocationChange.
   *
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser to wait for the location change on.
   * @param {string} url
   *        The string URL to look for. The URL must match the URL in the
   *        location bar exactly.
   * @return {Promise}
   * @resolves When onLocationChange fires.
   */
  waitForLocationChange(tabbrowser, url) {
    return new Promise((resolve, reject) => {
      let progressListener = {
        onLocationChange(aBrowser) {
          if ((url && aBrowser.currentURI.spec != url) ||
              (!url && aBrowser.currentURI.spec == "about:blank")) {
            return;
          }

          tabbrowser.removeTabsProgressListener(progressListener);
          resolve();
        },
      };
      tabbrowser.addTabsProgressListener(progressListener);
    });
  },

  /**
   * Waits for the next browser window to open and be fully loaded.
   *
   * @param aParams
   *        {
   *          url: A string (optional). If set, we will wait until the initial
   *               browser in the new window has loaded a particular page.
   *               If unset, the initial browser may or may not have finished
   *               loading its first page when the resulting Promise resolves.
   *          anyWindow: True to wait for the url to be loaded in any new
   *                     window, not just the next one opened.
   *          maybeErrorPage: See browserLoaded function.
   *        }
   * @return {Promise}
   *         A Promise which resolves the next time that a DOM window
   *         opens and the delayed startup observer notification fires.
   */
  waitForNewWindow(aParams = {}) {
    let {
      url = null,
      anyWindow = false,
      maybeErrorPage = false,
    } = aParams;

    if (anyWindow && !url) {
      throw new Error("url should be specified if anyWindow is true");
    }

    return new Promise(resolve => {
      let observe = async (win, topic, data) => {
        if (topic != "domwindowopened") {
          return;
        }

        if (!anyWindow) {
          Services.ww.unregisterNotification(observe);
        }

        // Add these event listeners now since they may fire before the
        // DOMContentLoaded event down below.
        let promises = [
          this.waitForEvent(win, "focus", true),
          this.waitForEvent(win, "activate"),
        ];

        if (url) {
          await this.waitForEvent(win, "DOMContentLoaded");

          if (win.document.documentURI != AppConstants.BROWSER_CHROME_URL) {
            return;
          }
        }

        promises.push(TestUtils.topicObserved("browser-delayed-startup-finished",
                                              subject => subject == win));

        if (url) {
          let browser = win.gBrowser.selectedBrowser;

          if (win.gMultiProcessBrowser &&
              !E10SUtils.canLoadURIInRemoteType(url, browser.remoteType)) {
            await this.waitForEvent(browser, "XULFrameLoaderCreated");
          }

          let loadPromise = this.browserLoaded(browser, false, url, maybeErrorPage);
          promises.push(loadPromise);
        }

        await Promise.all(promises);

        if (anyWindow) {
          Services.ww.unregisterNotification(observe);
        }
        resolve(win);
      };
      Services.ww.registerNotification(observe);
    });
  },

  /**
   * Loads a new URI in the given browser and waits until we really started
   * loading. In e10s browser.loadURI() can be an asynchronous operation due
   * to having to switch the browser's remoteness and keep its shistory data.
   *
   * @param {xul:browser} browser
   *        A xul:browser.
   * @param {string} uri
   *        The URI to load.
   *
   * @return {Promise}
   * @resolves When we started loading the given URI.
   */
  async loadURI(browser, uri) {
    // Load the new URI.
    browser.loadURI(uri, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    // Nothing to do in non-e10s mode.
    if (!browser.ownerGlobal.gMultiProcessBrowser) {
      return;
    }

    // If the new URI can't load in the browser's current process then we
    // should wait for the new frameLoader to be created. This will happen
    // asynchronously when the browser's remoteness changes.
    if (!E10SUtils.canLoadURIInRemoteType(uri, browser.remoteType)) {
      await this.waitForEvent(browser, "XULFrameLoaderCreated");
    }
  },

  /**
   * @param win (optional)
   *        The window we should wait to have "domwindowopened" sent through
   *        the observer service for. If this is not supplied, we'll just
   *        resolve when the first "domwindowopened" notification is seen.
   * @param {function} checkFn [optional]
   *        Called with the nsIDOMWindow object as argument, should return true
   *        if the event is the expected one, or false if it should be ignored
   *        and observing should continue. If not specified, the first window
   *        resolves the returned promise.
   * @return {Promise}
   *         A Promise which resolves when a "domwindowopened" notification
   *         has been fired by the window watcher.
   */
  domWindowOpened(win, checkFn) {
    return new Promise(resolve => {
      async function observer(subject, topic, data) {
        if (topic == "domwindowopened" && (!win || subject === win)) {
          let observedWindow = subject.QueryInterface(Ci.nsIDOMWindow);
          if (checkFn && !await checkFn(observedWindow)) {
            return;
          }
          Services.ww.unregisterNotification(observer);
          resolve(observedWindow);
        }
      }
      Services.ww.registerNotification(observer);
    });
  },

  /**
   * @param win (optional)
   *        The window we should wait to have "domwindowclosed" sent through
   *        the observer service for. If this is not supplied, we'll just
   *        resolve when the first "domwindowclosed" notification is seen.
   * @return {Promise}
   *         A Promise which resolves when a "domwindowclosed" notification
   *         has been fired by the window watcher.
   */
  domWindowClosed(win) {
    return new Promise((resolve) => {
      function observer(subject, topic, data) {
        if (topic == "domwindowclosed" && (!win || subject === win)) {
          Services.ww.unregisterNotification(observer);
          resolve(subject.QueryInterface(Ci.nsIDOMWindow));
        }
      }
      Services.ww.registerNotification(observer);
    });
  },

  /**
   * Open a new browser window from an existing one.
   * This relies on OpenBrowserWindow in browser.js, and waits for the window
   * to be completely loaded before resolving.
   *
   * @param {Object}
   *        Options to pass to OpenBrowserWindow. Additionally, supports:
   *        - waitForTabURL
   *          Forces the initial browserLoaded check to wait for the tab to
   *          load the given URL (instead of about:blank)
   *
   * @return {Promise}
   *         Resolves with the new window once it is loaded.
   */
  async openNewBrowserWindow(options = {}) {
    let currentWin = BrowserWindowTracker.getTopWindow({private: false});
    if (!currentWin) {
      throw new Error("Can't open a new browser window from this helper if no non-private window is open.");
    }
    let win = currentWin.OpenBrowserWindow(options);

    let promises = [this.waitForEvent(win, "focus", true), this.waitForEvent(win, "activate")];

    // Wait for browser-delayed-startup-finished notification, it indicates
    // that the window has loaded completely and is ready to be used for
    // testing.
    promises.push(
      TestUtils.topicObserved("browser-delayed-startup-finished",
                              subject => subject == win).then(() => win)
    );


    promises.push(this.firstBrowserLoaded(win, !options.waitForTabURL, browser => {
      return !options.waitForTabURL || options.waitForTabURL == browser.currentURI.spec;
    }));

    await Promise.all(promises);

    return win;
  },

  /**
   * Closes a window.
   *
   * @param {Window}
   *        A window to close.
   *
   * @return {Promise}
   *         Resolves when the provided window has been closed. For browser
   *         windows, the Promise will also wait until all final SessionStore
   *         messages have been sent up from all browser tabs.
   */
  closeWindow(win) {
    let closedPromise = BrowserTestUtils.windowClosed(win);
    win.close();
    return closedPromise;
  },

  /**
   * Returns a Promise that resolves when a window has finished closing.
   *
   * @param {Window}
   *        The closing window.
   *
   * @return {Promise}
   *        Resolves when the provided window has been fully closed. For
   *        browser windows, the Promise will also wait until all final
   *        SessionStore messages have been sent up from all browser tabs.
   */
  windowClosed(win) {
    let domWinClosedPromise = BrowserTestUtils.domWindowClosed(win);
    let promises = [domWinClosedPromise];
    let winType = win.document.documentElement.getAttribute("windowtype");

    if (winType == "navigator:browser") {
      let finalMsgsPromise = new Promise((resolve) => {
        let browserSet = new Set(win.gBrowser.browsers);
        // Ensure all browsers have been inserted or we won't get
        // messages back from them.
        browserSet.forEach((browser) => {
          win.gBrowser._insertBrowser(win.gBrowser.getTabForBrowser(browser));
        });
        let mm = win.getGroupMessageManager("browsers");

        mm.addMessageListener("SessionStore:update", function onMessage(msg) {
          if (browserSet.has(msg.target) && msg.data.isFinal) {
            browserSet.delete(msg.target);
            if (!browserSet.size) {
              mm.removeMessageListener("SessionStore:update", onMessage);
              // Give the TabStateFlusher a chance to react to this final
              // update and for the TabStateFlusher.flushWindow promise
              // to resolve before we resolve.
              TestUtils.executeSoon(resolve);
            }
          }
        }, true);
      });

      promises.push(finalMsgsPromise);
    }

    return Promise.all(promises);
  },

  /**
   * Returns a Promise that resolves once the SessionStore information for the
   * given tab is updated and all listeners are called.
   *
   * @param (tab) tab
   *        The tab that will be removed.
   * @returns (Promise)
   * @resolves When the SessionStore information is updated.
   */
  waitForSessionStoreUpdate(tab) {
    return new Promise(resolve => {
      let {messageManager: mm, frameLoader} = tab.linkedBrowser;
      mm.addMessageListener("SessionStore:update", function onMessage(msg) {
        if (msg.targetFrameLoader == frameLoader && msg.data.isFinal) {
          mm.removeMessageListener("SessionStore:update", onMessage);
          // Wait for the next event tick to make sure other listeners are
          // called.
          TestUtils.executeSoon(() => resolve());
        }
      }, true);
    });
  },

  /**
   * Waits for an event to be fired on a specified element.
   *
   * Usage:
   *    let promiseEvent = BrowserTestUtils.waitForEvent(element, "eventName");
   *    // Do some processing here that will cause the event to be fired
   *    // ...
   *    // Now wait until the Promise is fulfilled
   *    let receivedEvent = await promiseEvent;
   *
   * The promise resolution/rejection handler for the returned promise is
   * guaranteed not to be called until the next event tick after the event
   * listener gets called, so that all other event listeners for the element
   * are executed before the handler is executed.
   *
   *    let promiseEvent = BrowserTestUtils.waitForEvent(element, "eventName");
   *    // Same event tick here.
   *    await promiseEvent;
   *    // Next event tick here.
   *
   * If some code, such like adding yet another event listener, needs to be
   * executed in the same event tick, use raw addEventListener instead and
   * place the code inside the event listener.
   *
   *    element.addEventListener("load", () => {
   *      // Add yet another event listener in the same event tick as the load
   *      // event listener.
   *      p = BrowserTestUtils.waitForEvent(element, "ready");
   *    }, { once: true });
   *
   * @param {Element} subject
   *        The element that should receive the event.
   * @param {string} eventName
   *        Name of the event to listen to.
   * @param {bool} capture [optional]
   *        True to use a capturing listener.
   * @param {function} checkFn [optional]
   *        Called with the Event object as argument, should return true if the
   *        event is the expected one, or false if it should be ignored and
   *        listening should continue. If not specified, the first event with
   *        the specified name resolves the returned promise.
   * @param {bool} wantsUntrusted [optional]
   *        True to receive synthetic events dispatched by web content.
   *
   * @note Because this function is intended for testing, any error in checkFn
   *       will cause the returned promise to be rejected instead of waiting for
   *       the next event, since this is probably a bug in the test.
   *
   * @returns {Promise}
   * @resolves The Event object.
   */
  waitForEvent(subject, eventName, capture, checkFn, wantsUntrusted) {
    return new Promise((resolve, reject) => {
      subject.addEventListener(eventName, function listener(event) {
        try {
          if (checkFn && !checkFn(event)) {
            return;
          }
          subject.removeEventListener(eventName, listener, capture);
          TestUtils.executeSoon(() => resolve(event));
        } catch (ex) {
          try {
            subject.removeEventListener(eventName, listener, capture);
          } catch (ex2) {
            // Maybe the provided object does not support removeEventListener.
          }
          TestUtils.executeSoon(() => reject(ex));
        }
      }, capture, wantsUntrusted);
    });
  },

  /**
   * Like waitForEvent, but adds the event listener to the message manager
   * global for browser.
   *
   * @param {string} eventName
   *        Name of the event to listen to.
   * @param {bool} capture [optional]
   *        Whether to use a capturing listener.
   * @param {function} checkFn [optional]
   *        Called with the Event object as argument, should return true if the
   *        event is the expected one, or false if it should be ignored and
   *        listening should continue. If not specified, the first event with
   *        the specified name resolves the returned promise.
   * @param {bool} wantsUntrusted [optional]
   *        Whether to accept untrusted events
   *
   * @note Because this function is intended for testing, any error in checkFn
   *       will cause the returned promise to be rejected instead of waiting for
   *       the next event, since this is probably a bug in the test.
   *
   * @returns {Promise}
   */
  waitForContentEvent(browser, eventName, capture = false, checkFn, wantsUntrusted = false) {
    let parameters = {
      eventName,
      capture,
      checkFnSource: checkFn ? checkFn.toSource() : null,
      wantsUntrusted,
    };
    /* eslint-disable no-eval */
    return ContentTask.spawn(browser, parameters,
        function({ eventName, capture, checkFnSource, wantsUntrusted }) {
          let checkFn;
          if (checkFnSource) {
            checkFn = eval(`(() => (${checkFnSource}))()`);
          }
          return new Promise((resolve, reject) => {
            addEventListener(eventName, function listener(event) {
              let completion = resolve;
              try {
                if (checkFn && !checkFn(event)) {
                  return;
                }
              } catch (e) {
                completion = () => reject(e);
              }
              removeEventListener(eventName, listener, capture);
              completion();
            }, capture, wantsUntrusted);
          });
        });
    /* eslint-enable no-eval */
  },

  /**
   * Adds a content event listener on the given browser
   * element. Similar to waitForContentEvent, but the listener will
   * fire until it is removed. A callable object is returned that,
   * when called, removes the event listener. Note that this function
   * works even if the browser's frameloader is swapped.
   *
   * @param {xul:browser} browser
   *        The browser element to listen for events in.
   * @param {string} eventName
   *        Name of the event to listen to.
   * @param {function} listener
   *        Function to call in parent process when event fires.
   *        Not passed any arguments.
   * @param {bool} useCapture [optional]
   *        Whether to use a capturing listener.
   * @param {function} checkFn [optional]
   *        Called with the Event object as argument, should return true if the
   *        event is the expected one, or false if it should be ignored and
   *        listening should continue. If not specified, the first event with
   *        the specified name resolves the returned promise. This is called
   *        within the content process and can have no closure environment.
   * @param {bool} wantsUntrusted [optional]
   *        Whether to accept untrusted events
   * @param {bool} autoremove [optional]
   *        Whether the listener should be removed when |browser| is removed
   *        from the DOM. Note that, if this flag is true, it won't be possible
   *        to listen for events after a frameloader swap.
   *
   * @returns function
   *        If called, the return value will remove the event listener.
   */
  addContentEventListener(browser,
                          eventName,
                          listener,
                          useCapture = false,
                          checkFn,
                          wantsUntrusted = false,
                          autoremove = true) {
    let id = gListenerId++;
    let checkFnSource = checkFn ? encodeURIComponent(escape(checkFn.toSource())) : "";

    // To correctly handle frameloader swaps, we load a frame script
    // into all tabs but ignore messages from the ones not related to
    // |browser|.

    /* eslint-disable no-eval */
    function frameScript(id, eventName, useCapture, checkFnSource, wantsUntrusted) {
      let checkFn;
      if (checkFnSource) {
        checkFn = eval(`(() => (${unescape(checkFnSource)}))()`);
      }

      function listener(event) {
        if (checkFn && !checkFn(event)) {
          return;
        }
        sendAsyncMessage("ContentEventListener:Run", id);
      }
      function removeListener(msg) {
        if (msg.data == id) {
          removeMessageListener("ContentEventListener:Remove", removeListener);
          removeEventListener(eventName, listener, useCapture, wantsUntrusted);
        }
      }
      addMessageListener("ContentEventListener:Remove", removeListener);
      addEventListener(eventName, listener, useCapture, wantsUntrusted);
    }
    /* eslint-enable no-eval */

    let frameScriptSource =
        `data:,(${frameScript.toString()})(${id}, "${eventName}", ${uneval(useCapture)}, "${checkFnSource}", ${wantsUntrusted})`;

    let mm = Services.mm;

    function runListener(msg) {
      if (msg.data == id && msg.target == browser) {
        listener();
      }
    }
    mm.addMessageListener("ContentEventListener:Run", runListener);

    let needCleanup = true;

    let unregisterFunction = function() {
      if (!needCleanup) {
        return;
      }
      needCleanup = false;
      mm.removeMessageListener("ContentEventListener:Run", runListener);
      mm.broadcastAsyncMessage("ContentEventListener:Remove", id);
      mm.removeDelayedFrameScript(frameScriptSource);
      if (autoremove) {
        Services.obs.removeObserver(cleanupObserver, "message-manager-close");
      }
    };

    function cleanupObserver(subject, topic, data) {
      if (subject == browser.messageManager) {
        unregisterFunction();
      }
    }
    if (autoremove) {
      Services.obs.addObserver(cleanupObserver, "message-manager-close");
    }

    mm.loadFrameScript(frameScriptSource, true);

    return unregisterFunction;
  },

  /**
   * Like browserLoaded, but waits for an error page to appear.
   * This explicitly deals with cases where the browser is not currently remote and a
   * remoteness switch will occur before the error page is loaded, which is tricky
   * because error pages don't fire 'regular' load events that we can rely on.
   *
   * @param {xul:browser} browser
   *        A xul:browser.
   *
   * @return {Promise}
   * @resolves When an error page has been loaded in the browser.
   */
  waitForErrorPage(browser) {
    let waitForLoad = () =>
      this.waitForContentEvent(browser, "AboutNetErrorLoad", false, null, true);

    let win = browser.ownerGlobal;
    let tab = win.gBrowser.getTabForBrowser(browser);
    if (!tab || browser.isRemoteBrowser || !win.gMultiProcessBrowser) {
      return waitForLoad();
    }

    // We're going to switch remoteness when loading an error page. We need to be
    // quite careful in order to make sure we're adding the listener in time to
    // get this event:
    return new Promise((resolve, reject) => {
      tab.addEventListener("TabRemotenessChange", function() {
        waitForLoad().then(resolve, reject);
      }, {once: true});
    });
  },

  /**
   *  Versions of EventUtils.jsm synthesizeMouse functions that synthesize a
   *  mouse event in a child process and return promises that resolve when the
   *  event has fired and completed. Instead of a window, a browser is required
   *  to be passed to this function.
   *
   * @param target
   *        One of the following:
   *        - a selector string that identifies the element to target. The syntax is as
   *          for querySelector.
   *        - An array of selector strings. Each selector after the first
   *          selects for an element in the iframe specified by the previous
   *          selector.
   *        - a CPOW element (for easier test-conversion).
   *        - a function to be run in the content process that returns the element to
   *        target
   *        - null, in which case the offset is from the content document's edge.
   * @param {integer} offsetX
   *        x offset from target's left bounding edge
   * @param {integer} offsetY
   *        y offset from target's top bounding edge
   * @param {Object} event object
   *        Additional arguments, similar to the EventUtils.jsm version
   * @param {Browser} browser
   *        Browser element, must not be null
   *
   * @returns {Promise}
   * @resolves True if the mouse event was cancelled.
   */
  synthesizeMouse(target, offsetX, offsetY, event, browser) {
    return new Promise((resolve, reject) => {
      let mm = browser.messageManager;
      mm.addMessageListener("Test:SynthesizeMouseDone", function mouseMsg(message) {
        mm.removeMessageListener("Test:SynthesizeMouseDone", mouseMsg);
        if (message.data.hasOwnProperty("defaultPrevented")) {
          resolve(message.data.defaultPrevented);
        } else {
          reject(new Error(message.data.error));
        }
      });

      let cpowObject = null;
      let targetFn = null;
      if (typeof target == "function") {
        targetFn = target.toString();
        target = null;
      } else if (typeof target != "string" && !Array.isArray(target)) {
        cpowObject = target;
        target = null;
      }

      mm.sendAsyncMessage("Test:SynthesizeMouse",
                          {target, targetFn, x: offsetX, y: offsetY, event},
                          {object: cpowObject});
    });
  },

  /**
   *  Versions of EventUtils.jsm synthesizeTouch functions that synthesize a
   *  touch event in a child process and return promises that resolve when the
   *  event has fired and completed. Instead of a window, a browser is required
   *  to be passed to this function.
   *
   * @param target
   *        One of the following:
   *        - a selector string that identifies the element to target. The syntax is as
   *          for querySelector.
   *        - An array of selector strings. Each selector after the first
   *          selects for an element in the iframe specified by the previous
   *          selector.
   *        - a CPOW element (for easier test-conversion).
   *        - a function to be run in the content process that returns the element to
   *        target
   *        - null, in which case the offset is from the content document's edge.
   * @param {integer} offsetX
   *        x offset from target's left bounding edge
   * @param {integer} offsetY
   *        y offset from target's top bounding edge
   * @param {Object} event object
   *        Additional arguments, similar to the EventUtils.jsm version
   * @param {Browser} browser
   *        Browser element, must not be null
   *
   * @returns {Promise}
   * @resolves True if the touch event was cancelled.
   */
  synthesizeTouch(target, offsetX, offsetY, event, browser) {
    return new Promise((resolve, reject) => {
      let mm = browser.messageManager;
      mm.addMessageListener("Test:SynthesizeTouchDone", function touchMsg(message) {
        mm.removeMessageListener("Test:SynthesizeTouchDone", touchMsg);
        if (message.data.hasOwnProperty("defaultPrevented")) {
          resolve(message.data.defaultPrevented);
        } else {
          reject(new Error(message.data.error));
        }
      });

      let cpowObject = null;
      let targetFn = null;
      if (typeof target == "function") {
        targetFn = target.toString();
        target = null;
      } else if (typeof target != "string" && !Array.isArray(target)) {
        cpowObject = target;
        target = null;
      }

      mm.sendAsyncMessage("Test:SynthesizeTouch",
                          {target, targetFn, x: offsetX, y: offsetY, event},
                          {object: cpowObject});
    });
  },

  /**
   * Wait for a message to be fired from a particular message manager
   *
   * @param {nsIMessageManager} messageManager
   *                            The message manager that should be used.
   * @param {String}            message
   *                            The message we're waiting for.
   * @param {Function}          checkFn (optional)
   *                            Optional function to invoke to check the message.
   */
  waitForMessage(messageManager, message, checkFn) {
    return new Promise(resolve => {
      messageManager.addMessageListener(message, function onMessage(msg) {
        if (!checkFn || checkFn(msg)) {
          messageManager.removeMessageListener(message, onMessage);
          resolve(msg.data);
        }
      });
    });
  },

  /**
   *  Version of synthesizeMouse that uses the center of the target as the mouse
   *  location. Arguments and the return value are the same.
   */
  synthesizeMouseAtCenter(target, event, browser) {
    // Use a flag to indicate to center rather than having a separate message.
    event.centered = true;
    return BrowserTestUtils.synthesizeMouse(target, 0, 0, event, browser);
  },

  /**
   *  Version of synthesizeMouse that uses a client point within the child
   *  window instead of a target as the offset. Otherwise, the arguments and
   *  return value are the same as synthesizeMouse.
   */
  synthesizeMouseAtPoint(offsetX, offsetY, event, browser) {
    return BrowserTestUtils.synthesizeMouse(null, offsetX, offsetY, event, browser);
  },

  /**
   * Removes the given tab from its parent tabbrowser.
   * This method doesn't SessionStore etc.
   *
   * @param (tab) tab
   *        The tab to remove.
   * @param (Object) options
   *        Extra options to pass to tabbrowser's removeTab method.
   */
  removeTab(tab, options = {}) {
    tab.ownerGlobal.gBrowser.removeTab(tab, options);
  },

  /**
   * Returns a Promise that resolves once the tab starts closing.
   *
   * @param (tab) tab
   *        The tab that will be removed.
   * @returns (Promise)
   * @resolves When the tab starts closing. Does not get passed a value.
   */
  waitForTabClosing(tab) {
    return this.waitForEvent(tab, "TabClose");
  },

  /**
   * Crashes a remote browser tab and cleans up the generated minidumps.
   * Resolves with the data from the .extra file (the crash annotations).
   *
   * @param (Browser) browser
   *        A remote <xul:browser> element. Must not be null.
   * @param (bool) shouldShowTabCrashPage
   *        True if it is expected that the tab crashed page will be shown
   *        for this browser. If so, the Promise will only resolve once the
   *        tab crash page has loaded.
   * @param (bool) shouldClearMinidumps
   *        True if the minidumps left behind by the crash should be removed.
   *
   * @returns (Promise)
   * @resolves An Object with key-value pairs representing the data from the
   *           crash report's extra file (if applicable).
   */
  async crashBrowser(browser, shouldShowTabCrashPage = true,
                     shouldClearMinidumps = true) {
    let extra = {};
    let KeyValueParser = {};
    if (AppConstants.MOZ_CRASHREPORTER) {
      ChromeUtils.import("resource://gre/modules/KeyValueParser.jsm", KeyValueParser);
    }

    if (!browser.isRemoteBrowser) {
      throw new Error("<xul:browser> needs to be remote in order to crash");
    }

    /**
     * Returns the directory where crash dumps are stored.
     *
     * @return nsIFile
     */
    function getMinidumpDirectory() {
      let dir = Services.dirsvc.get("ProfD", Ci.nsIFile);
      dir.append("minidumps");
      return dir;
    }

    /**
     * Removes a file from a directory. This is a no-op if the file does not
     * exist.
     *
     * @param directory
     *        The nsIFile representing the directory to remove from.
     * @param filename
     *        A string for the file to remove from the directory.
     */
    function removeFile(directory, filename) {
      let file = directory.clone();
      file.append(filename);
      if (file.exists()) {
        file.remove(false);
      }
    }

    // This frame script is injected into the remote browser, and used to
    // intentionally crash the tab. We crash by using js-ctypes and dereferencing
    // a bad pointer. The crash should happen immediately upon loading this
    // frame script.
    let frame_script = () => {
      ChromeUtils.import("resource://gre/modules/ctypes.jsm");

      let dies = function() {
        privateNoteIntentionalCrash();
        let zero = new ctypes.intptr_t(8);
        let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
        badptr.contents;
      };

      dump("\nEt tu, Brute?\n");
      dies();
    };

    let expectedPromises = [];

    let crashCleanupPromise = new Promise((resolve, reject) => {
      let observer = (subject, topic, data) => {
        if (topic != "ipc:content-shutdown") {
          reject("Received incorrect observer topic: " + topic);
          return;
        }
        if (!(subject instanceof Ci.nsIPropertyBag2)) {
          reject("Subject did not implement nsIPropertyBag2");
          return;
        }
        // we might see this called as the process terminates due to previous tests.
        // We are only looking for "abnormal" exits...
        if (!subject.hasKey("abnormal")) {
          dump("\nThis is a normal termination and isn't the one we are looking for...\n");
          return;
        }

        let dumpID;
        if (AppConstants.MOZ_CRASHREPORTER) {
          dumpID = subject.getPropertyAsAString("dumpID");
          if (!dumpID) {
            reject("dumpID was not present despite crash reporting being enabled");
            return;
          }
        }

        let removalPromise = Promise.resolve();

        if (dumpID) {
          removalPromise = Services.crashmanager.ensureCrashIsPresent(dumpID)
                                                .then(() => {
            let minidumpDirectory = getMinidumpDirectory();
            let extrafile = minidumpDirectory.clone();
            extrafile.append(dumpID + ".extra");
            if (extrafile.exists()) {
              dump(`\nNo .extra file for dumpID: ${dumpID}\n`);
              if (AppConstants.MOZ_CRASHREPORTER) {
                extra = KeyValueParser.parseKeyValuePairsFromFile(extrafile);
              } else {
                dump("\nCrashReporter not enabled - will not return any extra data\n");
              }
            }

            if (shouldClearMinidumps) {
              removeFile(minidumpDirectory, dumpID + ".dmp");
              removeFile(minidumpDirectory, dumpID + ".extra");
            }
          });
        }

        removalPromise.then(() => {
          Services.obs.removeObserver(observer, "ipc:content-shutdown");
          dump("\nCrash cleaned up\n");
          // There might be other ipc:content-shutdown handlers that need to
          // run before we want to continue, so we'll resolve on the next tick
          // of the event loop.
          TestUtils.executeSoon(() => resolve());
        });
      };

      Services.obs.addObserver(observer, "ipc:content-shutdown");
    });

    expectedPromises.push(crashCleanupPromise);

    if (shouldShowTabCrashPage) {
      expectedPromises.push(new Promise((resolve, reject) => {
        browser.addEventListener("AboutTabCrashedReady", function onCrash() {
          browser.removeEventListener("AboutTabCrashedReady", onCrash);
          dump("\nabout:tabcrashed loaded and ready\n");
          resolve();
        }, false, true);
      }));
    }

    // This frame script will crash the remote browser as soon as it is
    // evaluated.
    let mm = browser.messageManager;
    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);

    await Promise.all(expectedPromises);

    if (shouldShowTabCrashPage) {
      let gBrowser = browser.ownerGlobal.gBrowser;
      let tab = gBrowser.getTabForBrowser(browser);
      if (tab.getAttribute("crashed") != "true") {
        throw new Error("Tab should be marked as crashed");
      }
    }

    return extra;
  },

  /**
   * Returns a promise that is resolved when element gains attribute (or,
   * optionally, when it is set to value).
   * @param {String} attr
   *        The attribute to wait for
   * @param {Element} element
   *        The element which should gain the attribute
   * @param {String} value (optional)
   *        Optional, the value the attribute should have.
   *
   * @returns {Promise}
   */
  waitForAttribute(attr, element, value) {
    let MutationObserver = element.ownerGlobal.MutationObserver;
    return new Promise(resolve => {
      let mut = new MutationObserver(mutations => {
        if ((!value && element.getAttribute(attr)) ||
            (value && element.getAttribute(attr) === value)) {
          resolve();
          mut.disconnect();

        }
      });

      mut.observe(element, {attributeFilter: [attr]});
    });
  },

  /**
   * Version of EventUtils' `sendChar` function; it will synthesize a keypress
   * event in a child process and returns a Promise that will resolve when the
   * event was fired. Instead of a Window, a Browser object is required to be
   * passed to this function.
   *
   * @param {String} char
   *        A character for the keypress event that is sent to the browser.
   * @param {Browser} browser
   *        Browser element, must not be null.
   *
   * @returns {Promise}
   * @resolves True if the keypress event was synthesized.
   */
  sendChar(char, browser) {
    return new Promise(resolve => {
      let seq = ++gSendCharCount;
      let mm = browser.messageManager;

      mm.addMessageListener("Test:SendCharDone", function charMsg(message) {
        if (message.data.seq != seq)
          return;

        mm.removeMessageListener("Test:SendCharDone", charMsg);
        resolve(message.data.result);
      });

      mm.sendAsyncMessage("Test:SendChar", {
        char,
        seq,
      });
    });
  },

  /**
   * Version of EventUtils' `synthesizeKey` function; it will synthesize a key
   * event in a child process and returns a Promise that will resolve when the
   * event was fired. Instead of a Window, a Browser object is required to be
   * passed to this function.
   *
   * @param {String} key
   *        See the documentation available for EventUtils#synthesizeKey.
   * @param {Object} event
   *        See the documentation available for EventUtils#synthesizeKey.
   * @param {Browser} browser
   *        Browser element, must not be null.
   *
   * @returns {Promise}
   */
  synthesizeKey(key, event, browser) {
    return new Promise(resolve => {
      let seq = ++gSynthesizeKeyCount;
      let mm = browser.messageManager;

      mm.addMessageListener("Test:SynthesizeKeyDone", function keyMsg(message) {
        if (message.data.seq != seq)
          return;

        mm.removeMessageListener("Test:SynthesizeKeyDone", keyMsg);
        resolve();
      });

      mm.sendAsyncMessage("Test:SynthesizeKey", { key, event, seq });
    });
  },

  /**
   * Version of EventUtils' `synthesizeComposition` function; it will synthesize
   * a composition event in a child process and returns a Promise that will
   * resolve when the event was fired. Instead of a Window, a Browser object is
   * required to be passed to this function.
   *
   * @param {Object} event
   *        See the documentation available for EventUtils#synthesizeComposition.
   * @param {Browser} browser
   *        Browser element, must not be null.
   *
   * @returns {Promise}
   * @resolves False if the composition event could not be synthesized.
   */
  synthesizeComposition(event, browser) {
    return new Promise(resolve => {
      let seq = ++gSynthesizeCompositionCount;
      let mm = browser.messageManager;

      mm.addMessageListener("Test:SynthesizeCompositionDone", function compMsg(message) {
        if (message.data.seq != seq)
          return;

        mm.removeMessageListener("Test:SynthesizeCompositionDone", compMsg);
        resolve(message.data.result);
      });

      mm.sendAsyncMessage("Test:SynthesizeComposition", { event, seq });
    });
  },

  /**
   * Version of EventUtils' `synthesizeCompositionChange` function; it will
   * synthesize a compositionchange event in a child process and returns a
   * Promise that will resolve when the event was fired. Instead of a Window, a
   * Browser object is required to be passed to this function.
   *
   * @param {Object} event
   *        See the documentation available for EventUtils#synthesizeCompositionChange.
   * @param {Browser} browser
   *        Browser element, must not be null.
   *
   * @returns {Promise}
   */
  synthesizeCompositionChange(event, browser) {
    return new Promise(resolve => {
      let seq = ++gSynthesizeCompositionChangeCount;
      let mm = browser.messageManager;

      mm.addMessageListener("Test:SynthesizeCompositionChangeDone", function compMsg(message) {
        if (message.data.seq != seq)
          return;

        mm.removeMessageListener("Test:SynthesizeCompositionChangeDone", compMsg);
        resolve();
      });

      mm.sendAsyncMessage("Test:SynthesizeCompositionChange", { event, seq });
    });
  },

  // TODO: Fix consumers and remove me.
  waitForCondition: TestUtils.waitForCondition,

  /**
   * Waits for a <xul:notification> with a particular value to appear
   * for the <xul:notificationbox> of the passed in browser.
   *
   * @param tabbrowser (<xul:tabbrowser>)
   *        The gBrowser that hosts the browser that should show
   *        the notification. For most tests, this will probably be
   *        gBrowser.
   * @param browser (<xul:browser>)
   *        The browser that should be showing the notification.
   * @param notificationValue (string)
   *        The "value" of the notification, which is often used as
   *        a unique identifier. Example: "plugin-crashed".
   * @return Promise
   *        Resolves to the <xul:notification> that is being shown.
   */
  waitForNotificationBar(tabbrowser, browser, notificationValue) {
    let notificationBox = tabbrowser.getNotificationBox(browser);
    return this.waitForNotificationInNotificationBox(notificationBox,
                                                     notificationValue);
  },

  /**
   * Waits for a <xul:notification> with a particular value to appear
   * in the global <xul:notificationbox> of the given browser window.
   *
   * @param win (<xul:window>)
   *        The browser window in whose global notificationbox the
   *        notification is expected to appear.
   * @param notificationValue (string)
   *        The "value" of the notification, which is often used as
   *        a unique identifier. Example: "captive-portal-detected".
   * @return Promise
   *        Resolves to the <xul:notification> that is being shown.
   */
  waitForGlobalNotificationBar(win, notificationValue) {
    return this.waitForNotificationInNotificationBox(
                       win.gHighPriorityNotificationBox, notificationValue);
  },

  waitForNotificationInNotificationBox(notificationBox, notificationValue) {
    return new Promise((resolve) => {
      let check = (event) => {
        return event.target.getAttribute("value") == notificationValue;
      };

      BrowserTestUtils.waitForEvent(notificationBox.stack, "AlertActive",
                                    false, check).then((event) => {
        // The originalTarget of the AlertActive on a notificationbox
        // will be the notification itself.
        resolve(event.originalTarget);
      });
    });
  },

  /**
   * Returns a Promise that will resolve once MozAfterPaint
   * has been fired in the content of a browser.
   *
   * @param browser (<xul:browser>)
   *        The browser for which we're waiting for the MozAfterPaint
   *        event to occur in.
   * @returns Promise
   */
  contentPainted(browser) {
    return ContentTask.spawn(browser, null, async function() {
      return new Promise((resolve) => {
        addEventListener("MozAfterPaint", function onPaint() {
          removeEventListener("MozAfterPaint", onPaint);
          resolve();
        });
      });
    });
  },

  _knownAboutPages: new Set(),
  _loadedAboutContentScript: false,
  /**
   * Registers an about: page with particular flags in both the parent
   * and any content processes. Returns a promise that resolves when
   * registration is complete.
   *
   * @param registerCleanupFunction (Function)
   *        The test framework doesn't keep its cleanup stuff anywhere accessible,
   *        so the first argument is a reference to your cleanup registration
   *        function, allowing us to clean up after you if necessary.
   * @param aboutModule (String)
   *        The name of the about page.
   * @param pageURI (String)
   *        The URI the about: page should point to.
   * @param flags (Number)
   *        The nsIAboutModule flags to use for registration.
   * @returns Promise that resolves when registration has finished.
   */
  registerAboutPage(registerCleanupFunction, aboutModule, pageURI, flags) {
    // Return a promise that resolves when registration finished.
    const kRegistrationMsgId = "browser-test-utils:about-registration:registered";
    let rv = this.waitForMessage(Services.ppmm, kRegistrationMsgId, msg => {
      return msg.data == aboutModule;
    });
    // Load a script that registers our page, then send it a message to execute the registration.
    if (!this._loadedAboutContentScript) {
      Services.ppmm.loadProcessScript(kAboutPageRegistrationContentScript, true);
      this._loadedAboutContentScript = true;
      registerCleanupFunction(this._removeAboutPageRegistrations.bind(this));
    }
    Services.ppmm.broadcastAsyncMessage("browser-test-utils:about-registration:register",
                                        {aboutModule, pageURI, flags});
    return rv.then(() => {
      this._knownAboutPages.add(aboutModule);
    });
  },

  unregisterAboutPage(aboutModule) {
    if (!this._knownAboutPages.has(aboutModule)) {
      return Promise.reject(new Error("We don't think this about page exists!"));
    }
    const kUnregistrationMsgId = "browser-test-utils:about-registration:unregistered";
    let rv = this.waitForMessage(Services.ppmm, kUnregistrationMsgId, msg => {
      return msg.data == aboutModule;
    });
    Services.ppmm.broadcastAsyncMessage("browser-test-utils:about-registration:unregister",
                                        aboutModule);
    return rv.then(() => this._knownAboutPages.delete(aboutModule));
  },

  async _removeAboutPageRegistrations() {
    for (let aboutModule of this._knownAboutPages) {
      await this.unregisterAboutPage(aboutModule);
    }
    Services.ppmm.removeDelayedProcessScript(kAboutPageRegistrationContentScript);
  },

  /**
   * Waits for the dialog to open, and clicks the specified button.
   *
   * @param {string} buttonAction
   *        The ID of the button to click ("accept", "cancel", etc).
   * @param {string} uri
   *        The URI of the dialog to wait for.  Defaults to the common dialog.
   * @return {Promise}
   *         A Promise which resolves when a "domwindowopened" notification
   *         for a dialog has been fired by the window watcher and the
   *         specified button is clicked.
   */
  async promiseAlertDialogOpen(buttonAction,
                               uri = "chrome://global/content/commonDialog.xul",
                               func) {
    let win = await this.domWindowOpened(null, async win => {
      // The test listens for the "load" event which guarantees that the alert
      // class has already been added (it is added when "DOMContentLoaded" is
      // fired).
      await this.waitForEvent(win, "load");

      return win.document.documentURI === uri;
    });

    if (func) {
      await func(win);
      return win;
    }

    let doc = win.document.documentElement;
    doc.getButton(buttonAction).click();

    return win;
  },

  /**
   * Waits for the dialog to open, and clicks the specified button, and waits
   * for the dialog to close.
   *
   * @param {string} buttonAction
   *        The ID of the button to click ("accept", "cancel", etc).
   * @param {string} uri
   *        The URI of the dialog to wait for.  Defaults to the common dialog.
   * @return {Promise}
   *         A Promise which resolves when a "domwindowopened" notification
   *         for a dialog has been fired by the window watcher and the
   *         specified button is clicked, and the dialog has been fully closed.
   */
  async promiseAlertDialog(buttonAction,
                           uri = "chrome://global/content/commonDialog.xul",
                           func) {
    let win = await this.promiseAlertDialogOpen(buttonAction, uri, func);
    return this.windowClosed(win);
  },

  /**
   * Opens a tab with a given uri and params object. If the params object is not set
   * or the params parameter does not include a triggeringPricnipal then this function
   * provides a params object using the systemPrincipal as the default triggeringPrincipal.
   *
   * @param {xul:tabbrowser} tabbrowser
   *        The gBrowser object to open the tab with.
   * @param {string} uri
   *        The URI to open in the new tab.
   * @param {object} params [optional]
   *        Parameters object for gBrowser.addTab.
   * @param {function} beforeLoadFunc [optional]
   *        A function to run after that xul:browser has been created but before the URL is
   *        loaded. Can spawn a content task in the tab, for example.
   */
  addTab(tabbrowser, uri, params = {}, beforeLoadFunc = null) {
    if (!params.triggeringPrincipal) {
      params.triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    }
    if (!params.allowInheritPrincipal) {
      params.allowInheritPrincipal = true;
    }
    if (beforeLoadFunc) {
      let window = tabbrowser.ownerGlobal;
      window.addEventListener("TabOpen", function(e) {
        beforeLoadFunc(e.target);
      }, {once: true});
    }
    return tabbrowser.addTab(uri, params);
  },
};
