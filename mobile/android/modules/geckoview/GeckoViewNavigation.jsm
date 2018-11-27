/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewNavigation"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Utils: "resource://gre/modules/sessionstore/Utils.jsm",
  LoadURIDelegate: "resource://gre/modules/LoadURIDelegate.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// Handles navigation requests between Gecko and a GeckoView.
// Handles GeckoView:GoBack and :GoForward requests dispatched by
// GeckoView.goBack and .goForward.
// Dispatches GeckoView:LocationChange to the GeckoView on location change when
// active.
// Implements nsIBrowserDOMWindow.
class GeckoViewNavigation extends GeckoViewModule {
  onInitBrowser() {
    this.window.browserDOMWindow = this;

    // There may be a GeckoViewNavigation module in another window waiting for
    // us to create a browser so it can call presetOpenerWindow(), so allow them
    // to do that now.
    Services.obs.notifyObservers(this.window, "geckoview-window-created");
  }

  onInit() {
    this.registerListener([
      "GeckoView:GoBack",
      "GeckoView:GoForward",
      "GeckoView:LoadUri",
      "GeckoView:Reload",
      "GeckoView:Stop",
    ]);

    this.messageManager.addMessageListener("Browser:LoadURI", this);
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug `onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:GoBack":
        this.browser.goBack();
        break;
      case "GeckoView:GoForward":
        this.browser.goForward();
        break;
      case "GeckoView:LoadUri":
        const { uri, referrer, flags } = aData;

        let navFlags = 0;

        // These need to match the values in GeckoSession.LOAD_FLAGS_*
        if (flags & (1 << 0)) {
          navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        }

        if (flags & (1 << 1)) {
          navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY;
        }

        if (flags & (1 << 2)) {
          navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_EXTERNAL;
        }

        if (flags & (1 << 3)) {
          navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_POPUPS;
        }

        if (flags & (1 << 4)) {
          navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER;
        }

        if (this.settings.useMultiprocess) {
          this.moduleManager.updateRemoteTypeForURI(uri);
        }

        let parsedUri;
        let triggeringPrincipal;
        try {
            parsedUri = Services.io.newURI(uri);
            if (parsedUri.schemeIs("about") || parsedUri.schemeIs("data") ||
                parsedUri.schemeIs("file") || parsedUri.schemeIs("resource")) {
              // Only allow privileged loading for certain URIs.
              triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
            }
        } catch (ignored) {
        }
        if (!triggeringPrincipal) {
          triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal({});
        }

        this.browser.loadURI(parsedUri ? parsedUri.spec : uri, {
          flags: navFlags,
          referrerURI: referrer,
          triggeringPrincipal,
        });
        break;
      case "GeckoView:Reload":
        this.browser.reload();
        break;
      case "GeckoView:Stop":
        this.browser.stop();
        break;
    }
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug `receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "Browser:LoadURI":
        // This is triggered by E10SUtils.redirectLoad(), and means
        // we may need to change the remoteness of our browser and
        // load the URI.
        const { uri, flags, referrer, triggeringPrincipal } = aMsg.data.loadOptions;

        this.moduleManager.updateRemoteTypeForURI(uri);

        this.browser.loadURI(uri, {
          flags,
          referrerURI: referrer,
          triggeringPrincipal: Utils.deserializePrincipal(triggeringPrincipal),
        });
        break;
    }
  }

  waitAndSetupWindow(aSessionId, { opener, nextTabParentId }) {
    if (!aSessionId) {
      return Promise.resolve(null);
    }

    return new Promise(resolve => {
      const handler = {
        observe(aSubject, aTopic, aData) {
          if (aTopic === "geckoview-window-created" && aSubject.name === aSessionId) {
            if (nextTabParentId) {
              aSubject.browser.setAttribute("nextTabParentId", nextTabParentId.toString());
            }

            if (opener) {
              if (aSubject.browser.hasAttribute("remote")) {
                // We cannot start in remote mode when we have an opener.
                aSubject.browser.setAttribute("remote", "false");
                aSubject.browser.removeAttribute("remoteType");
              }
              aSubject.browser.presetOpenerWindow(opener);
            }
            Services.obs.removeObserver(handler, "geckoview-window-created");
            resolve(aSubject);
          }
        },
      };

      // This event is emitted from createBrowser() in geckoview.js
      Services.obs.addObserver(handler, "geckoview-window-created");
    });
  }

  handleNewSession(aUri, aOpener, aWhere, aFlags, aNextTabParentId) {
    debug `handleNewSession: uri=${aUri && aUri.spec}
                             where=${aWhere} flags=${aFlags}`;

    if (!this.enabled) {
      return null;
    }

    const message = {
      type: "GeckoView:OnNewSession",
      uri: aUri ? aUri.displaySpec : "",
    };

    let browser = undefined;
    this.eventDispatcher.sendRequestForResult(message).then(sessionId => {
      return this.waitAndSetupWindow(sessionId, {
        opener: (aFlags & Ci.nsIBrowserDOMWindow.OPEN_NO_OPENER) ? null : aOpener,
        nextTabParentId: aNextTabParentId,
      });
    }).then(window => {
      browser = (window && window.browser) || null;
    }, () => {
      browser = null;
    });

    // Wait indefinitely for app to respond with a browser or null
    Services.tm.spinEventLoopUntil(() =>
        this.window.closed || browser !== undefined);
    return browser || null;
  }

  // nsIBrowserDOMWindow.
  createContentWindow(aUri, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    debug `createContentWindow: uri=${aUri && aUri.spec}
                                where=${aWhere} flags=${aFlags}`;

    if (LoadURIDelegate.load(this.window, this.eventDispatcher,
                             aUri, aWhere, aFlags, aTriggeringPrincipal)) {
      // The app has handled the load, abort open-window handling.
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    const browser = this.handleNewSession(aUri, aOpener, aWhere, aFlags, null);
    if (!browser) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    return browser.contentWindow;
  }

  // nsIBrowserDOMWindow.
  createContentWindowInFrame(aUri, aParams, aWhere, aFlags, aNextTabParentId,
                             aName) {
    debug `createContentWindowInFrame: uri=${aUri && aUri.spec}
                                       where=${aWhere} flags=${aFlags}
                                       nextTabParentId=${aNextTabParentId}
                                       name=${aName}`;

    if (LoadURIDelegate.load(this.window, this.eventDispatcher,
                             aUri, aWhere, aFlags,
                             aParams.triggeringPrincipal)) {
      // The app has handled the load, abort open-window handling.
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    const browser = this.handleNewSession(aUri, null, aWhere, aFlags, aNextTabParentId);
    if (!browser) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    return browser;
  }

  handleOpenUri(aUri, aOpener, aWhere, aFlags, aTriggeringPrincipal,
                aNextTabParentId) {
    debug `handleOpenUri: uri=${aUri && aUri.spec}
                          where=${aWhere} flags=${aFlags}`;

    if (LoadURIDelegate.load(this.window, this.eventDispatcher,
                             aUri, aWhere, aFlags, aTriggeringPrincipal)) {
      return null;
    }

    let browser = this.browser;

    if (aWhere === Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW ||
        aWhere === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB ||
        aWhere === Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB) {
      browser = this.handleNewSession(aUri, aOpener, aWhere, aFlags,
                                      aTriggeringPrincipal);
    }

    if (!browser) {
      // Should we throw?
      return null;
    }
    browser.loadURI(aUri.spec, null, null, null, null, aTriggeringPrincipal);
    return browser;
  }

  // nsIBrowserDOMWindow.
  openURI(aUri, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    const browser = this.handleOpenUri(aUri, aOpener, aWhere, aFlags,
                                       aTriggeringPrincipal, null);
    return browser && browser.contentWindow;
  }

  // nsIBrowserDOMWindow.
  openURIInFrame(aUri, aParams, aWhere, aFlags, aNextTabParentId, aName) {
    const browser = this.handleOpenUri(aUri, null, aWhere, aFlags,
                                       aParams.triggeringPrincipal,
                                       aNextTabParentId);
    return browser;
  }

  // nsIBrowserDOMWindow.
  isTabContentWindow(aWindow) {
    return this.browser.contentWindow === aWindow;
  }

  // nsIBrowserDOMWindow.
  canClose() {
    debug `canClose`;
    return true;
  }

  onEnable() {
    debug `onEnable`;

    let flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  onDisable() {
    debug `onDisable`;

    if (!this.progressFilter) {
      return;
    }
    this.progressFilter.removeProgressListener(this);
    this.browser.removeProgressListener(this.progressFilter);
  }

  // WebProgress event handler.
  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug `onLocationChange`;

    let fixedURI = aLocationURI;

    try {
      fixedURI = Services.uriFixup.createExposableURI(aLocationURI);
    } catch (ex) { }

    let message = {
      type: "GeckoView:LocationChange",
      uri: fixedURI.displaySpec,
      canGoBack: this.browser.canGoBack,
      canGoForward: this.browser.canGoForward,
      isTopLevel: aWebProgress.isTopLevel,
    };

    this.eventDispatcher.sendRequest(message);
  }
}
