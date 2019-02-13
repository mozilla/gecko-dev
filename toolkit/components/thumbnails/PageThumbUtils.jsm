/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Common thumbnailing routines used by various consumers, including
 * PageThumbs and backgroundPageThumbsContent.
 */

this.EXPORTED_SYMBOLS = ["PageThumbUtils"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm", this);

this.PageThumbUtils = {
  // The default background color for page thumbnails.
  THUMBNAIL_BG_COLOR: "#fff",
  // The namespace for thumbnail canvas elements.
  HTML_NAMESPACE: "http://www.w3.org/1999/xhtml",

  /**
   * Creates a new canvas element in the context of aWindow, or if aWindow
   * is undefined, in the context of hiddenDOMWindow.
   *
   * @param aWindow (optional) The document of this window will be used to
   *  create the canvas.  If not given, the hidden window will be used.
   * @return The newly created canvas.
   */
  createCanvas: function (aWindow) {
    let doc = (aWindow || Services.appShell.hiddenDOMWindow).document;
    let canvas = doc.createElementNS(this.HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.mozImageSmoothingEnabled = true;
    let [thumbnailWidth, thumbnailHeight] = this.getThumbnailSize();
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    return canvas;
  },

  /**
   * Calculates a preferred initial thumbnail size based on current desktop
   * dimensions. The resulting dims will generally be about 1/3 the
   * size of the desktop. (jimm: why??)
   *
   * @return The calculated thumbnail size or a default if unable to calculate.
   */
  getThumbnailSize: function () {
    if (!this._thumbnailWidth || !this._thumbnailHeight) {
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                            .getService(Ci.nsIScreenManager);
      let left = {}, top = {}, width = {}, height = {};
      screenManager.primaryScreen.GetRectDisplayPix(left, top, width, height);
      this._thumbnailWidth = Math.round(width.value / 3);
      this._thumbnailHeight = Math.round(height.value / 3);
    }
    return [this._thumbnailWidth, this._thumbnailHeight];
  },

  /**
   * Determine a good thumbnail crop size and scale for a given content
   * window.
   *
   * @param aWindow The content window.
   * @param aCanvas The target canvas.
   * @return An array containing width, height and scale.
   */
  determineCropSize: function (aWindow, aCanvas) {
    if (Cu.isCrossProcessWrapper(aWindow)) {
      throw new Error('Do not pass cpows here.');
    }
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    // aWindow may be a cpow, add exposed props security values.
    let sbWidth = {}, sbHeight = {};

    try {
      utils.getScrollbarSize(false, sbWidth, sbHeight);
    } catch (e) {
      // This might fail if the window does not have a presShell.
      Cu.reportError("Unable to get scrollbar size in determineCropSize.");
      sbWidth.value = sbHeight.value = 0;
    }

    // Even in RTL mode, scrollbars are always on the right.
    // So there's no need to determine a left offset.
    let width = aWindow.innerWidth - sbWidth.value;
    let height = aWindow.innerHeight - sbHeight.value;

    let {width: thumbnailWidth, height: thumbnailHeight} = aCanvas;
    let scale = Math.min(Math.max(thumbnailWidth / width, thumbnailHeight / height), 1);
    let scaledWidth = width * scale;
    let scaledHeight = height * scale;

    if (scaledHeight > thumbnailHeight)
      height -= Math.floor(Math.abs(scaledHeight - thumbnailHeight) * scale);

    if (scaledWidth > thumbnailWidth)
      width -= Math.floor(Math.abs(scaledWidth - thumbnailWidth) * scale);

    return [width, height, scale];
  },

  shouldStoreContentThumbnail: function (aDocument, aDocShell) {
    // FIXME Bug 720575 - Don't capture thumbnails for SVG or XML documents as
    //       that currently regresses Talos SVG tests.
    if (aDocument instanceof Ci.nsIDOMXMLDocument) {
      return false;
    }

    let webNav = aDocShell.QueryInterface(Ci.nsIWebNavigation);

    // Don't take screenshots of about: pages.
    if (webNav.currentURI.schemeIs("about")) {
      return false;
    }

    // There's no point in taking screenshot of loading pages.
    if (aDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE) {
      return false;
    }

    let channel = aDocShell.currentDocumentChannel;

    // No valid document channel. We shouldn't take a screenshot.
    if (!channel) {
      return false;
    }

    // Don't take screenshots of internally redirecting about: pages.
    // This includes error pages.
    let uri = channel.originalURI;
    if (uri.schemeIs("about")) {
      return false;
    }

    let httpChannel;
    try {
      httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (e) { /* Not an HTTP channel. */ }

    if (httpChannel) {
      // Continue only if we have a 2xx status code.
      try {
        if (Math.floor(httpChannel.responseStatus / 100) != 2) {
        return false;
        }
      } catch (e) {
        // Can't get response information from the httpChannel
        // because mResponseHead is not available.
        return false;
      }

      // Cache-Control: no-store.
      if (httpChannel.isNoStoreResponse()) {
        return false;
      }

      // Don't capture HTTPS pages unless the user explicitly enabled it.
      if (uri.schemeIs("https") &&
          !Services.prefs.getBoolPref("browser.cache.disk_cache_ssl")) {
        return false;
      }
    } // httpChannel
    return true;
  }
};
