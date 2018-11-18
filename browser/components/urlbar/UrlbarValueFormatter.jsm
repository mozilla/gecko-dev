/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarValueFormatter"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
});

/**
 * Applies URL highlighting and other styling to the text in the urlbar input,
 * depending on the text.
 */
class UrlbarValueFormatter {
  /**
   * @param {UrlbarInput} urlbarInput
   */
  constructor(urlbarInput) {
    this.urlbarInput = urlbarInput;
    this.window = this.urlbarInput.window;
    this.document = this.window.document;

    // This is used only as an optimization to avoid removing formatting in
    // the _remove* format methods when no formatting is actually applied.
    this._formattingApplied = false;
  }

  get inputField() {
    return this.urlbarInput.inputField;
  }

  get scheme() {
    return this.document.getAnonymousElementByAttribute(
      this.urlbarInput.textbox, "anonid", "scheme");
  }

  update() {
    if (!this.inputField.value) {
      return;
    }

    // Remove the current formatting.
    this._removeURLFormat();
    this._removeSearchAliasFormat();

    // Apply new formatting.  Formatter methods should return true if they
    // successfully formatted the value and false if not.  We apply only
    // one formatter at a time, so we stop at the first successful one.
    this._formattingApplied =
      this._formatURL() ||
      this._formatSearchAlias();
  }

  ensureFormattedHostVisible(urlMetaData) {
    // Used to avoid re-entrance in the requestAnimationFrame callback.
    let instance = this._formatURLInstance = {};

    // Make sure the host is always visible. Since it is aligned on
    // the first strong directional character, we set scrollLeft
    // appropriately to ensure the domain stays visible in case of an
    // overflow.
    this.window.requestAnimationFrame(() => {
      // Check for re-entrance. On focus change this formatting code is
      // invoked regardless, thus this should be enough.
      if (this._formatURLInstance != instance) {
        return;
      }

      // In the future, for example in bug 525831, we may add a forceRTL
      // char just after the domain, and in such a case we should not
      // scroll to the left.
      urlMetaData = urlMetaData || this._getUrlMetaData();
      if (!urlMetaData) {
        return;
      }
      let { url, preDomain, domain } = urlMetaData;
      let directionality = this.window.windowUtils.getDirectionFromText(domain);
      if (directionality == this.window.windowUtils.DIRECTION_RTL &&
          url[preDomain.length + domain.length] != "\u200E") {
        this.inputField.scrollLeft = this.inputField.scrollLeftMax;
      }
    });
  }

  _getUrlMetaData() {
    if (this.urlbarInput.focused) {
      return null;
    }

    let url = this.inputField.value;

    // Get the URL from the fixup service:
    let flags = Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
                Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let uriInfo;
    try {
      uriInfo = Services.uriFixup.getFixupURIInfo(url, flags);
    } catch (ex) {}
    // Ignore if we couldn't make a URI out of this, the URI resulted in a search,
    // or the URI has a non-http(s)/ftp protocol.
    if (!uriInfo ||
        !uriInfo.fixedURI ||
        uriInfo.keywordProviderName ||
        !["http", "https", "ftp"].includes(uriInfo.fixedURI.scheme)) {
      return null;
    }

    // If we trimmed off the http scheme, ensure we stick it back on before
    // trying to figure out what domain we're accessing, so we don't get
    // confused by user:pass@host http URLs. We later use
    // trimmedLength to ensure we don't count the length of a trimmed protocol
    // when determining which parts of the URL to highlight as "preDomain".
    let trimmedLength = 0;
    if (uriInfo.fixedURI.scheme == "http" && !url.startsWith("http://")) {
      url = "http://" + url;
      trimmedLength = "http://".length;
    }

    let matchedURL = url.match(/^(([a-z]+:\/\/)(?:[^\/#?]+@)?)(\S+?)(?::\d+)?\s*(?:[\/#?]|$)/);
    if (!matchedURL) {
      return null;
    }

    let [, preDomain, schemeWSlashes, domain] = matchedURL;
    return { preDomain, schemeWSlashes, domain, url, uriInfo, trimmedLength };
  }

  _removeURLFormat() {
    this.scheme.value = "";
    if (!this._formattingApplied) {
      return;
    }
    let controller = this.urlbarInput.editor.selectionController;
    let strikeOut =
      controller.getSelection(controller.SELECTION_URLSTRIKEOUT);
    strikeOut.removeAllRanges();
    let selection =
      controller.getSelection(controller.SELECTION_URLSECONDARY);
    selection.removeAllRanges();
    this._formatScheme(controller.SELECTION_URLSTRIKEOUT, true);
    this._formatScheme(controller.SELECTION_URLSECONDARY, true);
    this.inputField.style.setProperty("--urlbar-scheme-size", "0px");
  }

  /**
   * If the input value is a URL and the input is not focused, this
   * formatter method highlights the domain, and if mixed content is present,
   * it crosses out the https scheme.  It also ensures that the host is
   * visible (not scrolled out of sight).
   *
   * @returns {boolean}
   *   True if formatting was applied and false if not.
   */
  _formatURL() {
    let urlMetaData = this._getUrlMetaData();
    if (!urlMetaData) {
      return false;
    }

    let { url, uriInfo, preDomain, schemeWSlashes, domain, trimmedLength } = urlMetaData;
    // We strip http, so we should not show the scheme box for it.
    if (!UrlbarPrefs.get("trimURLs") || schemeWSlashes != "http://") {
      this.scheme.value = schemeWSlashes;
      this.inputField.style.setProperty("--urlbar-scheme-size",
                                        schemeWSlashes.length + "ch");
    }

    this.ensureFormattedHostVisible(urlMetaData);

    if (!UrlbarPrefs.get("formatting.enabled")) {
      return false;
    }

    let editor = this.urlbarInput.editor;
    let controller = editor.selectionController;

    this._formatScheme(controller.SELECTION_URLSECONDARY);

    let textNode = editor.rootElement.firstChild;

    // Strike out the "https" part if mixed active content is loaded.
    if (this.urlbarInput.getAttribute("pageproxystate") == "valid" &&
        url.startsWith("https:") &&
        this.window.gBrowser.securityUI.state &
          Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT) {
      let range = this.document.createRange();
      range.setStart(textNode, 0);
      range.setEnd(textNode, 5);
      let strikeOut =
        controller.getSelection(controller.SELECTION_URLSTRIKEOUT);
      strikeOut.addRange(range);
      this._formatScheme(controller.SELECTION_URLSTRIKEOUT);
    }

    let baseDomain = domain;
    let subDomain = "";
    try {
      baseDomain = Services.eTLD.getBaseDomainFromHost(uriInfo.fixedURI.host);
      if (!domain.endsWith(baseDomain)) {
        // getBaseDomainFromHost converts its resultant to ACE.
        let IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
        baseDomain = IDNService.convertACEtoUTF8(baseDomain);
      }
    } catch (e) {}
    if (baseDomain != domain) {
      subDomain = domain.slice(0, -baseDomain.length);
    }

    let selection = controller.getSelection(controller.SELECTION_URLSECONDARY);

    let rangeLength = preDomain.length + subDomain.length - trimmedLength;
    if (rangeLength) {
      let range = this.document.createRange();
      range.setStart(textNode, 0);
      range.setEnd(textNode, rangeLength);
      selection.addRange(range);
    }

    let startRest = preDomain.length + domain.length - trimmedLength;
    if (startRest < url.length - trimmedLength) {
      let range = this.document.createRange();
      range.setStart(textNode, startRest);
      range.setEnd(textNode, url.length - trimmedLength);
      selection.addRange(range);
    }

    return true;
  }

  _formatScheme(selectionType, clear) {
    let editor = this.scheme.editor;
    let controller = editor.selectionController;
    let textNode = editor.rootElement.firstChild;
    let selection = controller.getSelection(selectionType);
    if (clear) {
      selection.removeAllRanges();
    } else {
      let r = this.document.createRange();
      r.setStart(textNode, 0);
      r.setEnd(textNode, textNode.textContent.length);
      selection.addRange(r);
    }
  }

  _removeSearchAliasFormat() {
    if (!this._formattingApplied) {
      return;
    }
    let selection = this.urlbarInput.editor.selectionController.getSelection(
      Ci.nsISelectionController.SELECTION_FIND
    );
    selection.removeAllRanges();
  }

  /**
   * If the input value starts with an @engine search alias, this highlights it.
   *
   * @returns {boolean}
   *   True if formatting was applied and false if not.
   */
  _formatSearchAlias() {
    if (!UrlbarPrefs.get("formatting.enabled")) {
      return false;
    }

    let popup = this.urlbarInput.popup;
    if (!popup) {
      // TODO: make this work with UrlbarView
      return false;
    }

    if (popup.oneOffSearchButtons.selectedButton) {
      return false;
    }

    let editor = this.urlbarInput.editor;
    let textNode = editor.rootElement.firstChild;
    let value = textNode.textContent;
    let trimmedValue = value.trim();

    if (!trimmedValue.startsWith("@")) {
      return false;
    }

    // To determine whether the input contains a valid alias, check the value of
    // the selected result -- whether it's a search engine result with an alias.
    // Actually, check the selected listbox item, not the result in the
    // controller, because we want to continue highlighting the alias when the
    // popup is closed and the search has stopped.  The selected index when the
    // popup is closed is zero, however, which is why we also check the previous
    // selected index.
    let itemIndex =
      popup.selectedIndex < 0 ? popup._previousSelectedIndex :
      popup.selectedIndex;
    if (itemIndex < 0) {
      return false;
    }
    let item = popup.richlistbox.children[itemIndex] || null;

    // This actiontype check isn't necessary because we call _parseActionUrl
    // below and we could check action.type instead.  But since this method is
    // called very often, as an optimization, first do a simple string
    // comparison on actiontype before continuing with the more expensive regexp
    // that _parseActionUrl uses.
    if (!item || item.getAttribute("actiontype") != "searchengine") {
      return false;
    }

    let url = item.getAttribute("url");
    let action = this.urlbarInput._parseActionUrl(url);
    if (!action) {
      return false;
    }
    let alias = action.params.alias || null;
    if (!alias) {
      return false;
    }

    // Make sure the item's input matches the current urlbar input because the
    // urlbar input can change without the popup results changing.  Most notably
    // that happens when the user performs a search using an alias: The popup
    // closes (preserving its items), the search results page is loaded, and the
    // urlbar value is set to the URL of the page.
    //
    // If the item is the heuristic item, then its input is the value that the
    // user has typed in the input.  If the item is not the heuristic item, then
    // its input is "@engine ".  So in order to make sure the item's input
    // matches the current urlbar input, we need to check that the urlbar input
    // starts with the item's input.
    if (!trimmedValue.startsWith(action.params.input.trim())) {
      return false;
    }

    let index = value.indexOf(alias);
    if (index < 0) {
      return false;
    }

    // We abuse the SELECTION_FIND selection type to do our highlighting.
    // It's the only type that works with Selection.setColors().
    let selection = editor.selectionController.getSelection(
      Ci.nsISelectionController.SELECTION_FIND
    );

    let range = this.document.createRange();
    range.setStart(textNode, index);
    range.setEnd(textNode, index + alias.length);
    selection.addRange(range);

    let fg = "#2362d7";
    let bg = "#d2e6fd";

    // Selection.setColors() will swap the given foreground and background
    // colors if it detects that the contrast between the background
    // color and the frame color is too low.  Normally we don't want that
    // to happen; we want it to use our colors as given (even if setColors
    // thinks the contrast is too low).  But it's a nice feature for non-
    // default themes, where the contrast between our background color and
    // the input's frame color might actually be too low.  We can
    // (hackily) force setColors to use our colors as given by passing
    // them as the alternate colors.  Otherwise, allow setColors to swap
    // them, which we can do by passing "currentColor".  See
    // nsTextPaintStyle::GetHighlightColors for details.
    if (this.document.documentElement.querySelector(":-moz-lwtheme") ||
        (AppConstants.platform == "win" &&
         this.window.matchMedia("(-moz-windows-default-theme: 0)").matches)) {
      // non-default theme(s)
      selection.setColors(fg, bg, "currentColor", "currentColor");
    } else {
      // default themes
      selection.setColors(fg, bg, fg, bg);
    }

    return true;
  }
}
