/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
  FirefoxRelay: "resource://gre/modules/FirefoxRelay.sys.mjs",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});

export class ContextMenuParent extends JSWindowActorParent {
  receiveMessage(message) {
    let browser = this.manager.rootFrameLoader.ownerElement;
    let win = browser.ownerGlobal;
    // It's possible that the <xul:browser> associated with this
    // ContextMenu message doesn't belong to a window that actually
    // loads nsContextMenu.js. In that case, try to find the chromeEventHandler,
    // since that'll likely be the "top" <xul:browser>, and then use its window's
    // nsContextMenu instance instead.
    if (!win.nsContextMenu) {
      let topBrowser = browser.ownerGlobal.docShell.chromeEventHandler;
      win = topBrowser.ownerGlobal;
    }

    message.data.context.showRelay &&= lazy.FirefoxRelay.isEnabled;

    this.#openContextMenu(message.data, win, browser);
  }

  hiding() {
    try {
      this.sendAsyncMessage("ContextMenu:Hiding", {});
    } catch (e) {
      // This will throw if the content goes away while the
      // context menu is still open.
    }
  }

  reloadFrame(targetIdentifier, forceReload) {
    this.sendAsyncMessage("ContextMenu:ReloadFrame", {
      targetIdentifier,
      forceReload,
    });
  }

  getImageText(targetIdentifier) {
    return this.sendQuery("ContextMenu:GetImageText", {
      targetIdentifier,
    });
  }

  toggleRevealPassword(targetIdentifier) {
    this.sendAsyncMessage("ContextMenu:ToggleRevealPassword", {
      targetIdentifier,
    });
  }

  async useRelayMask(targetIdentifier, origin) {
    if (!origin) {
      return;
    }

    const windowGlobal = this.manager.browsingContext.currentWindowGlobal;
    const browser = windowGlobal.rootFrameLoader.ownerElement;
    const emailMask = await lazy.FirefoxRelay.generateUsername(browser, origin);
    if (emailMask) {
      this.sendAsyncMessage("ContextMenu:UseRelayMask", {
        targetIdentifier,
        emailMask,
      });
    }
  }

  reloadImage(targetIdentifier) {
    this.sendAsyncMessage("ContextMenu:ReloadImage", { targetIdentifier });
  }

  getFrameTitle(targetIdentifier) {
    return this.sendQuery("ContextMenu:GetFrameTitle", { targetIdentifier });
  }

  mediaCommand(targetIdentifier, command, data) {
    let windowGlobal = this.manager.browsingContext.currentWindowGlobal;
    let browser = windowGlobal.rootFrameLoader.ownerElement;
    let win = browser.ownerGlobal;
    let windowUtils = win.windowUtils;
    this.sendAsyncMessage("ContextMenu:MediaCommand", {
      targetIdentifier,
      command,
      data,
      handlingUserInput: windowUtils.isHandlingUserInput,
    });
  }

  canvasToBlobURL(targetIdentifier) {
    return this.sendQuery("ContextMenu:Canvas:ToBlobURL", { targetIdentifier });
  }

  saveVideoFrameAsImage(targetIdentifier) {
    return this.sendQuery("ContextMenu:SaveVideoFrameAsImage", {
      targetIdentifier,
    });
  }

  setAsDesktopBackground(targetIdentifier) {
    return this.sendQuery("ContextMenu:SetAsDesktopBackground", {
      targetIdentifier,
    });
  }

  getSearchFieldBookmarkData(targetIdentifier) {
    return this.sendQuery("ContextMenu:SearchFieldBookmarkData", {
      targetIdentifier,
    });
  }

  /**
   * Handles opening of the context menu for the appropraite browser.
   *
   * @param {object} data
   *   The data for the context menu, received from the child.
   * @param {DOMWindow} win
   *   The window in which the context menu is to be opened.
   * @param {Browser} browser
   *   The browser the context menu is being opened for.
   */
  #openContextMenu(data, win, browser) {
    if (lazy.BrowserHandler.kiosk) {
      // Don't display context menus in kiosk mode
      return;
    }
    let wgp = this.manager;

    if (!wgp.isCurrentGlobal) {
      // Don't display context menus for unloaded documents
      return;
    }

    // NOTE: We don't use `wgp.documentURI` here as we want to use the failed
    // channel URI in the case we have loaded an error page.
    let documentURIObject = wgp.browsingContext.currentURI;

    let frameReferrerInfo = data.frameReferrerInfo;
    if (frameReferrerInfo) {
      frameReferrerInfo =
        lazy.E10SUtils.deserializeReferrerInfo(frameReferrerInfo);
    }

    let linkReferrerInfo = data.linkReferrerInfo;
    if (linkReferrerInfo) {
      linkReferrerInfo =
        lazy.E10SUtils.deserializeReferrerInfo(linkReferrerInfo);
    }

    let frameID = lazy.WebNavigationFrames.getFrameId(wgp.browsingContext);

    win.nsContextMenu.contentData = {
      context: data.context,
      browser,
      actor: this,
      editFlags: data.editFlags,
      spellInfo: data.spellInfo,
      principal: wgp.documentPrincipal,
      storagePrincipal: wgp.documentStoragePrincipal,
      documentURIObject,
      docLocation: documentURIObject.spec,
      charSet: data.charSet,
      referrerInfo: lazy.E10SUtils.deserializeReferrerInfo(data.referrerInfo),
      frameReferrerInfo,
      linkReferrerInfo,
      contentType: data.contentType,
      contentDisposition: data.contentDisposition,
      frameID,
      frameOuterWindowID: frameID,
      frameBrowsingContext: wgp.browsingContext,
      selectionInfo: data.selectionInfo,
      disableSetDesktopBackground: data.disableSetDesktopBackground,
      showRelay: data.showRelay,
      loginFillInfo: data.loginFillInfo,
      userContextId: wgp.browsingContext.originAttributes.userContextId,
      webExtContextData: data.webExtContextData,
      cookieJarSettings: wgp.cookieJarSettings,
    };

    let popup = win.document.getElementById("contentAreaContextMenu");
    let context = win.nsContextMenu.contentData.context;

    // Fill in some values in the context from the WindowGlobalParent actor.
    context.principal = wgp.documentPrincipal;
    context.storagePrincipal = wgp.documentStoragePrincipal;
    context.frameID = frameID;
    context.frameOuterWindowID = wgp.outerWindowId;
    context.frameBrowsingContextID = wgp.browsingContext.id;

    // We don't have access to the original event here, as that happened in
    // another process. Therefore we synthesize a new MouseEvent to propagate the
    // inputSource to the subsequently triggered popupshowing event.
    let newEvent = new PointerEvent("contextmenu", {
      bubbles: true,
      cancelable: true,
      screenX: context.screenXDevPx / win.devicePixelRatio,
      screenY: context.screenYDevPx / win.devicePixelRatio,
      button: 2,
      pointerType: (() => {
        switch (context.inputSource) {
          case MouseEvent.MOZ_SOURCE_MOUSE:
            return "mouse";
          case MouseEvent.MOZ_SOURCE_PEN:
            return "pen";
          case MouseEvent.MOZ_SOURCE_ERASER:
            return "eraser";
          case MouseEvent.MOZ_SOURCE_CURSOR:
            return "cursor";
          case MouseEvent.MOZ_SOURCE_TOUCH:
            return "touch";
          case MouseEvent.MOZ_SOURCE_KEYBOARD:
            return "keyboard";
          default:
            return "";
        }
      })(),
    });
    popup.openPopupAtScreen(newEvent.screenX, newEvent.screenY, true, newEvent);
  }
}
