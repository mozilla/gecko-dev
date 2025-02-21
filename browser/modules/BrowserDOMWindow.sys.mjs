/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserWindowTracker } from "resource:///modules/BrowserWindowTracker.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { PrivateBrowsingUtils } from "resource://gre/modules/PrivateBrowsingUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  URILoadingHelper: "resource:///modules/URILoadingHelper.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "loadDivertedInBackground",
  "browser.tabs.loadDivertedInBackground"
);

ChromeUtils.defineLazyGetter(lazy, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

/**
 * This class is instantiated once for each browser window, and the instance
 * is exposed as a `browserDOMWindow` property on that window.
 *
 * It implements the nsIBrowserDOMWindow interface, which is used by C++ as
 * well as toolkit code to have an application-agnostic interface to do things
 * like opening new tabs and windows. Fenix (Firefox on Android) has its own
 * implementation of the same interface.
 */
export class BrowserDOMWindow {
  /**
   * @type {Window}
   */
  win = null;

  constructor(win) {
    this.win = win;
  }

  static setupInWindow(win) {
    win.browserDOMWindow = new BrowserDOMWindow(win);
  }

  static teardownInWindow(win) {
    win.browserDOMWindow = null;
  }

  #openURIInNewTab(
    aURI,
    aReferrerInfo,
    aIsPrivate,
    aIsExternal,
    aForceNotRemote = false,
    aUserContextId = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID,
    aOpenWindowInfo = null,
    aOpenerBrowser = null,
    aTriggeringPrincipal = null,
    aName = "",
    aCsp = null,
    aSkipLoad = false,
    aWhere = undefined
  ) {
    let win, needToFocusWin;

    // try the current window.  if we're in a popup, fall back on the most recent browser window
    if (this.win.toolbar.visible) {
      win = this.win;
    } else {
      win = BrowserWindowTracker.getTopWindow({ private: aIsPrivate });
      needToFocusWin = true;
    }

    if (!win) {
      // we couldn't find a suitable window, a new one needs to be opened.
      return null;
    }

    if (aIsExternal && (!aURI || aURI.spec == "about:blank")) {
      win.BrowserCommands.openTab(); // this also focuses the location bar
      win.focus();
      return win.gBrowser.selectedBrowser;
    }

    // OPEN_NEWTAB_BACKGROUND and OPEN_NEWTAB_FOREGROUND are used by
    // `window.open` with modifiers.
    // The last case is OPEN_NEWTAB, which is used by:
    //   * a link with `target="_blank"`, without modifiers
    //   * `window.open` without features, without modifiers
    let loadInBackground;
    if (aWhere === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_BACKGROUND) {
      loadInBackground = true;
    } else if (aWhere === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_FOREGROUND) {
      loadInBackground = false;
    } else {
      loadInBackground = lazy.loadDivertedInBackground;
    }

    let tab = win.gBrowser.addTab(aURI ? aURI.spec : "about:blank", {
      triggeringPrincipal: aTriggeringPrincipal,
      referrerInfo: aReferrerInfo,
      userContextId: aUserContextId,
      fromExternal: aIsExternal,
      inBackground: loadInBackground,
      forceNotRemote: aForceNotRemote,
      openWindowInfo: aOpenWindowInfo,
      openerBrowser: aOpenerBrowser,
      name: aName,
      csp: aCsp,
      skipLoad: aSkipLoad,
    });
    let browser = win.gBrowser.getBrowserForTab(tab);

    if (needToFocusWin || (!loadInBackground && aIsExternal)) {
      win.focus();
    }

    return browser;
  }

  createContentWindow(
    aURI,
    aOpenWindowInfo,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp
  ) {
    return this.getContentWindowOrOpenURI(
      null,
      aOpenWindowInfo,
      aWhere,
      aFlags,
      aTriggeringPrincipal,
      aCsp,
      true
    );
  }

  openURI(aURI, aOpenWindowInfo, aWhere, aFlags, aTriggeringPrincipal, aCsp) {
    if (!aURI) {
      console.error("openURI should only be called with a valid URI");
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
    return this.getContentWindowOrOpenURI(
      aURI,
      aOpenWindowInfo,
      aWhere,
      aFlags,
      aTriggeringPrincipal,
      aCsp,
      false
    );
  }

  getContentWindowOrOpenURI(
    aURI,
    aOpenWindowInfo,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp,
    aSkipLoad
  ) {
    var browsingContext = null;
    var isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    var guessUserContextIdEnabled =
      isExternal &&
      !Services.prefs.getBoolPref(
        "browser.link.force_default_user_context_id_for_external_opens",
        false
      );
    var openingUserContextId =
      (guessUserContextIdEnabled &&
        lazy.URILoadingHelper.guessUserContextId(aURI)) ||
      Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;

    if (aOpenWindowInfo && isExternal) {
      console.error(
        "BrowserDOMWindow.openURI did not expect aOpenWindowInfo to be " +
          "passed if the context is OPEN_EXTERNAL."
      );
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use --chrome command-line option to load external chrome urls\n");
      return null;
    }

    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      if (
        isExternal &&
        Services.prefs.prefHasUserValue(
          "browser.link.open_newwindow.override.external"
        )
      ) {
        aWhere = Services.prefs.getIntPref(
          "browser.link.open_newwindow.override.external"
        );
      } else {
        aWhere = Services.prefs.getIntPref("browser.link.open_newwindow");
      }
    }

    let referrerInfo;
    if (aFlags & Ci.nsIBrowserDOMWindow.OPEN_NO_REFERRER) {
      referrerInfo = new lazy.ReferrerInfo(
        Ci.nsIReferrerInfo.EMPTY,
        false,
        null
      );
    } else if (
      aOpenWindowInfo &&
      aOpenWindowInfo.parent &&
      aOpenWindowInfo.parent.window
    ) {
      referrerInfo = new lazy.ReferrerInfo(
        aOpenWindowInfo.parent.window.document.referrerInfo.referrerPolicy,
        true,
        Services.io.newURI(aOpenWindowInfo.parent.window.location.href)
      );
    } else {
      referrerInfo = new lazy.ReferrerInfo(
        Ci.nsIReferrerInfo.EMPTY,
        true,
        null
      );
    }

    let isPrivate = aOpenWindowInfo
      ? aOpenWindowInfo.originAttributes.privateBrowsingId != 0
      : PrivateBrowsingUtils.isWindowPrivate(this.win);

    switch (aWhere) {
      case Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW: {
        // FIXME: Bug 408379. So how come this doesn't send the
        // referrer like the other loads do?
        var url = aURI && aURI.spec;
        let features = "all,dialog=no";
        if (isPrivate) {
          features += ",private";
        }
        // Pass all params to openDialog to ensure that "url" isn't passed through
        // loadOneOrMoreURIs, which splits based on "|"
        try {
          let extraOptions = Cc[
            "@mozilla.org/hash-property-bag;1"
          ].createInstance(Ci.nsIWritablePropertyBag2);
          extraOptions.setPropertyAsBool("fromExternal", isExternal);

          this.win.openDialog(
            AppConstants.BROWSER_CHROME_URL,
            "_blank",
            features,
            // window.arguments
            url,
            extraOptions,
            null,
            null,
            null,
            null,
            null,
            null,
            aTriggeringPrincipal,
            null,
            aCsp,
            aOpenWindowInfo
          );
          // At this point, the new browser window is just starting to load, and
          // hasn't created the content <browser> that we should return.
          // If the caller of this function is originating in C++, they can pass a
          // callback in nsOpenWindowInfo and it will be invoked when the browsing
          // context for a newly opened window is ready.
          browsingContext = null;
        } catch (ex) {
          console.error(ex);
        }
        break;
      }
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB:
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_BACKGROUND:
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_FOREGROUND: {
        // If we have an opener, that means that the caller is expecting access
        // to the nsIDOMWindow of the opened tab right away. For e10s windows,
        // this means forcing the newly opened browser to be non-remote so that
        // we can hand back the nsIDOMWindow. DocumentLoadListener will do the
        // job of shuttling off the newly opened browser to run in the right
        // process once it starts loading a URI.
        let forceNotRemote = aOpenWindowInfo && !aOpenWindowInfo.isRemote;
        let userContextId = aOpenWindowInfo
          ? aOpenWindowInfo.originAttributes.userContextId
          : openingUserContextId;
        let browser = this.#openURIInNewTab(
          aURI,
          referrerInfo,
          isPrivate,
          isExternal,
          forceNotRemote,
          userContextId,
          aOpenWindowInfo,
          aOpenWindowInfo?.parent?.top.embedderElement,
          aTriggeringPrincipal,
          "",
          aCsp,
          aSkipLoad,
          aWhere
        );
        if (browser) {
          browsingContext = browser.browsingContext;
        }
        break;
      }
      case Ci.nsIBrowserDOMWindow.OPEN_PRINT_BROWSER: {
        let browser =
          this.win.PrintUtils.handleStaticCloneCreatedForPrint(aOpenWindowInfo);
        if (browser) {
          browsingContext = browser.browsingContext;
        }
        break;
      }
      default:
        // OPEN_CURRENTWINDOW or an illegal value
        browsingContext = this.win.gBrowser.selectedBrowser.browsingContext;
        if (aURI) {
          let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          if (isExternal) {
            loadFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
          } else if (!aTriggeringPrincipal.isSystemPrincipal) {
            // XXX this code must be reviewed and changed when bug 1616353
            // lands.
            loadFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIRST_LOAD;
          }
          // This should ideally be able to call loadURI with the actual URI.
          // However, that would bypass some styles of fixup (notably Windows
          // paths passed as "URI"s), so this needs some further thought. It
          // should be addressed in bug 1815509.
          this.win.gBrowser.fixupAndLoadURIString(aURI.spec, {
            triggeringPrincipal: aTriggeringPrincipal,
            csp: aCsp,
            loadFlags,
            referrerInfo,
          });
        }
        if (!lazy.loadDivertedInBackground) {
          this.win.focus();
        }
    }
    return browsingContext;
  }

  createContentWindowInFrame(aURI, aParams, aWhere, aFlags, aName) {
    // Passing a null-URI to only create the content window,
    // and pass true for aSkipLoad to prevent loading of
    // about:blank
    return this.getContentWindowOrOpenURIInFrame(
      null,
      aParams,
      aWhere,
      aFlags,
      aName,
      true
    );
  }

  openURIInFrame(aURI, aParams, aWhere, aFlags, aName) {
    return this.getContentWindowOrOpenURIInFrame(
      aURI,
      aParams,
      aWhere,
      aFlags,
      aName,
      false
    );
  }

  getContentWindowOrOpenURIInFrame(
    aURI,
    aParams,
    aWhere,
    aFlags,
    aName,
    aSkipLoad
  ) {
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_PRINT_BROWSER) {
      return this.win.PrintUtils.handleStaticCloneCreatedForPrint(
        aParams.openWindowInfo
      );
    }

    if (
      aWhere != Ci.nsIBrowserDOMWindow.OPEN_NEWTAB &&
      aWhere != Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_BACKGROUND &&
      aWhere != Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_FOREGROUND
    ) {
      dump("Error: openURIInFrame can only open in new tabs or print");
      return null;
    }

    var isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    var userContextId =
      aParams.openerOriginAttributes &&
      "userContextId" in aParams.openerOriginAttributes
        ? aParams.openerOriginAttributes.userContextId
        : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;

    return this.#openURIInNewTab(
      aURI,
      aParams.referrerInfo,
      aParams.isPrivate,
      isExternal,
      false,
      userContextId,
      aParams.openWindowInfo,
      aParams.openerBrowser,
      aParams.triggeringPrincipal,
      aName,
      aParams.csp,
      aSkipLoad,
      aWhere
    );
  }

  canClose() {
    return this.win.CanCloseWindow();
  }

  get tabCount() {
    return this.win.gBrowser.tabs.length;
  }
}

BrowserDOMWindow.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIBrowserDOMWindow",
]);
