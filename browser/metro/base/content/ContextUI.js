/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Fired when the tabtray is displayed
const kContextUITabsShowEvent = "MozContextUITabsShow";
// add more as needed...

/*
 * Manages context UI (navbar, tabbar, appbar) and track visibility. Also
 * tracks events that summon and hide the context UI.
 */
var ContextUI = {
  _expandable: true,
  _hidingId: 0,

  /*******************************************
   * init
   */

  init: function init() {
    Elements.browsers.addEventListener('URLChanged', this, true);
    Elements.browsers.addEventListener("AlertActive", this, true);
    Elements.browsers.addEventListener("AlertClose", this, true);
    Elements.panelUI.addEventListener('ToolPanelShown', this, false);
    Elements.panelUI.addEventListener('ToolPanelHidden', this, false);

    Elements.tray.addEventListener("mousemove", this, false);
    Elements.tray.addEventListener("mouseleave", this, false);

    window.addEventListener("touchstart", this, true);
    window.addEventListener("mousedown", this, true);
    window.addEventListener("MozEdgeUIStarted", this, true);
    window.addEventListener("MozEdgeUICanceled", this, true);
    window.addEventListener("MozEdgeUICompleted", this, true);
    window.addEventListener("keypress", this, true);
    window.addEventListener("KeyboardChanged", this, false);
    window.addEventListener("MozFlyoutPanelShowing", this, false);

    Elements.tray.addEventListener("transitionend", this, true);

    Appbar.init();
  },

  /*******************************************
   * Context UI state getters & setters
   */

  // any visiblilty
  get isVisible() {
    return this.navbarVisible || this.tabbarVisible || this.contextAppbarVisible;
  },

  // navbar visiblilty
  get navbarVisible() {
    return (Elements.navbar.hasAttribute("visible") ||
            Elements.navbar.hasAttribute("startpage"));
  },

  // tabbar visiblilty
  get tabbarVisible() {
    return Elements.tray.hasAttribute("expanded");
  },

  // appbar visiblilty
  get contextAppbarVisible() {
    return Elements.contextappbar.isShowing;
  },

  // currently not in use, for the always show tabs feature
  get isExpandable() { return this._expandable; },
  set isExpandable(aFlag) {
    this._expandable = aFlag;
    if (!this._expandable)
      this.dismiss();
  },

  /*******************************************
   * Public api
   */

  /*
   * Toggle the current nav UI state. Fires context ui events.
   */
  toggleNavUI: function () {
    // The navbar is forced open when the start page is visible. appbar.js
    // controls the "visible" property, and browser-ui controls the "startpage"
    // property. Hence we rely on the tabbar for current toggle state.
    if (this.tabbarVisible) {
      this.dismiss();
    } else {
      this.displayNavUI();
    }
  },

  /*
   * Show the nav and tabs bar. Returns true if any non-visible UI
   * was shown. Fires context ui events.
   */
  displayNavUI: function () {
    let shown = false;

    if (!this.navbarVisible) {
      BrowserUI.updateURI();
      this.displayNavbar();
      shown = true;
    }

    if (!this.tabbarVisible) {
      this.displayTabs();
      shown = true;
    }

    if (shown) {
      ContentAreaObserver.updateContentArea();
    }

    return shown;
  },

  /*
   * Dismiss any context UI currently visible. Returns true if any
   * visible UI was dismissed. Fires context ui events.
   */
  dismiss: function () {
    let dismissed = false;

    this._clearDelayedTimeout();

    // No assurances this will hide the nav bar. It may have the
    // 'startpage' property set. This removes the 'visible' property.
    if (this.navbarVisible) {
      BrowserUI.blurNavBar();
      this.dismissNavbar();
      dismissed = true;
    }
    if (this.tabbarVisible) {
      this.dismissTabs();
      dismissed = true;
    }
    if (Elements.contextappbar.isShowing) {
      this.dismissContextAppbar();
      dismissed = true;
    }

    if (dismissed) {
      ContentAreaObserver.updateContentArea();
    }

    return dismissed;
  },

  /*
   * Briefly show the tab bar and then hide it. Fires context ui events.
   */
  peekTabs: function peekTabs(aDelay) {
    if (!this.tabbarVisible)
      this.displayTabs();

    ContextUI.dismissTabsWithDelay(aDelay);
  },

  /*
   * Dismiss tab bar after a delay. Fires context ui events.
   */
  dismissTabsWithDelay: function (aDelay) {
    aDelay = aDelay || kForegroundTabAnimationDelay;
    this._clearDelayedTimeout();
    this._lastTimeoutDelay = aDelay;
    this._hidingId = setTimeout(function () {
        ContextUI.dismissTabs();
      }, aDelay);
  },

  // Display the nav bar
  displayNavbar: function () {
    Elements.navbar.show();
    ContentAreaObserver.updateContentArea();
  },

  // Display the tab tray
  displayTabs: function () {
    this._clearDelayedTimeout();
    this._setIsExpanded(true);
  },

  // Dismiss the navbar if visible.
  dismissNavbar: function dismissNavbar() {
    if (!BrowserUI.isStartTabVisible) {
      Elements.autocomplete.closePopup();
      Elements.navbar.dismiss();
      ContentAreaObserver.updateContentArea();
    }
  },

  // Dismiss the tabstray if visible.
  dismissTabs: function dimissTabs() {
    this._clearDelayedTimeout();
    this._setIsExpanded(false);
  },

  // Dismiss the appbar if visible.
  dismissContextAppbar: function dismissContextAppbar() {
    Elements.contextappbar.dismiss();
  },

  /*******************************************
   * Internal utils
   */

  // tabtray state
  _setIsExpanded: function _setIsExpanded(aFlag, setSilently) {
    // if the tray can't be expanded, don't expand it.
    if (!this.isExpandable || this.tabbarVisible == aFlag)
      return;

    if (aFlag)
      Elements.tray.setAttribute("expanded", "true");
    else
      Elements.tray.removeAttribute("expanded");

    if (!setSilently)
      this._fire(kContextUITabsShowEvent);
  },

  _clearDelayedTimeout: function _clearDelayedTimeout() {
    if (this._hidingId) {
      clearTimeout(this._hidingId);
      this._hidingId = 0;
      this._delayedHide = false;
    }
  },

  _resetDelayedTimeout: function () {
    this._hidingId = setTimeout(function () {
        ContextUI.dismissTabs();
      }, this._lastTimeoutDelay);
  },

  /*******************************************
   * Events
   */

  _onEdgeUIStarted: function(aEvent) {
    this._hasEdgeSwipeStarted = true;
    this._clearDelayedTimeout();
    this.toggleNavUI();
  },

  _onEdgeUICanceled: function(aEvent) {
    this._hasEdgeSwipeStarted = false;
    this.dismiss();
  },

  _onEdgeUICompleted: function(aEvent) {
    if (this._hasEdgeSwipeStarted) {
      this._hasEdgeSwipeStarted = false;
      return;
    }

    this._clearDelayedTimeout();
    this.toggleNavUI();
  },

  onDownInput: function onDownInput(aEvent) {
    if (!this.isVisible) {
      return;
    }

    // Various ui element containers we do not update context ui for.
    let whitelist = [
      // Clicks on tab bar elements should not close the tab bar. the tabbar
      // handles this.
      Elements.tabs,
      // Don't let a click on an infobar button dismiss the appbar or navbar.
      // Note the notification box should always hover above these other two
      // bars.
      Browser.getNotificationBox()
    ];

    if (whitelist.some(elem => elem.contains(aEvent.target))) {
      return;
    }

    // If a start tab is visible only dismiss the tab bar.
    if (BrowserUI.isStartTabVisible) {
      ContextUI.dismissTabs();
      return;
    }

    // content, dismiss anything visible
    if (aEvent.target.ownerDocument.defaultView.top == getBrowser().contentWindow) {
      this.dismiss();
      return;
    }

    // dismiss tabs and context app bar if visible
    this.dismissTabs();
    this.dismissContextAppbar();
  },

  onMouseMove: function (aEvent) {
    if (this._hidingId) {
      this._clearDelayedTimeout();
      this._delayedHide = true;
    }
  },

  onMouseLeave: function (aEvent) {
    if (this._delayedHide) {
      this._delayedHide = false;
      this._resetDelayedTimeout();
    }
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "URLChanged":
        // "aEvent.detail" is a boolean value that indicates whether actual URL
        // has changed ignoring URL fragment changes.
        if (aEvent.target == Browser.selectedBrowser && aEvent.detail) {
          this.displayNavbar();
        }
        break;
      case "MozEdgeUIStarted":
        this._onEdgeUIStarted(aEvent);
        break;
      case "MozEdgeUICanceled":
        this._onEdgeUICanceled(aEvent);
        break;
      case "MozEdgeUICompleted":
        this._onEdgeUICompleted(aEvent);
        break;
      case "keypress":
        if (String.fromCharCode(aEvent.which) == "z" &&
            aEvent.getModifierState("Win"))
          this.toggleNavUI();
        break;
      case "transitionend":
        setTimeout(function () {
          ContentAreaObserver.updateContentArea();
        }, 0);
        break;
      case "KeyboardChanged":
        this.dismissTabs();
        break;
      case "mousedown":
        if (aEvent.button != 0) {
          break;
        }
        this.onDownInput(aEvent);
        break;
      case "mousemove":
        this.onMouseMove(aEvent);
        break;
      case "mouseleave":
        this.onMouseLeave(aEvent);
        break;
      case "touchstart":
        this.onDownInput(aEvent);
        break;
      case "ToolPanelShown":
      case "ToolPanelHidden":
        this.dismiss();
        break;
      case "AlertActive":
      case "AlertClose":
        ContentAreaObserver.updateContentArea();
        break;
      case "MozFlyoutPanelShowing":
        if (BrowserUI.isStartTabVisible) {
          this.dismissTabs();
          this.dismissContextAppbar();
        } else {
          this.dismiss();
        }
        break;
    }
  },

  _fire: function (name) {
    let event = document.createEvent("Events");
    event.initEvent(name, true, true);
    window.dispatchEvent(event);
  }
};
