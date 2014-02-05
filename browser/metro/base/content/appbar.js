/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Appbar = {
  get starButton()    { return document.getElementById('star-button'); },
  get pinButton()     { return document.getElementById('pin-button'); },
  get menuButton()    { return document.getElementById('menu-button'); },

  // track selected/active richgrid/tilegroup - the context for contextual action buttons
  activeTileset: null,

  init: function Appbar_init() {
    // fired from appbar bindings
    Elements.contextappbar.addEventListener('MozAppbarShowing', this, false);
    Elements.contextappbar.addEventListener('MozAppbarDismissing', this, false);

    // fired when a context sensitive item (bookmarks) changes state
    window.addEventListener('MozContextActionsChange', this, false);

    // browser events we need to update button state on
    Elements.browsers.addEventListener('URLChanged', this, true);
    Elements.tabList.addEventListener('TabSelect', this, true);

    // tilegroup selection events for all modules get bubbled up
    window.addEventListener("selectionchange", this, false);
    Services.obs.addObserver(this, "metro_on_async_tile_created", false);

    // gather appbar telemetry data
    try {
      UITelemetry.addSimpleMeasureFunction("metro-appbar",
                                           this.getAppbarMeasures.bind(this));
    } catch (ex) {
      // swallow exception that occurs if metro-appbar measure is already set up
    }
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "metro_on_async_tile_created":
        this._updatePinButton();
        break;
    }
  },

  handleEvent: function Appbar_handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'URLChanged':
      case 'TabSelect':
        this.update();
        this.flushActiveTileset(aEvent.lastTab);
        break;

      case 'MozAppbarShowing':
        this.update();
        break;

      case 'MozAppbarDismissing':
        if (this.activeTileset && ('isBound' in this.activeTileset)) {
          this.activeTileset.clearSelection();
        }
        this._clearContextualActions();
        this.activeTileset = null;
        break;

      case 'MozContextActionsChange':
        let actions = aEvent.actions;
        let setName = aEvent.target.contextSetName;
        // could transition in old, new buttons?
        this.showContextualActions(actions, setName);
        break;

      case "selectionchange":
        let nodeName = aEvent.target.nodeName;
        if ('richgrid' === nodeName) {
          this._onTileSelectionChanged(aEvent);
        }
        break;
    }
  },

  flushActiveTileset: function flushActiveTileset(aTab) {
    try {
      let tab = aTab || Browser.selectedTab;
      // Switching away from or loading a site into a startui tab that has actions
      // pending, we consider this confirmation that the user wants to flush changes.
      if (this.activeTileset && tab && tab.browser && tab.browser.currentURI.spec == kStartURI) {
        ContextUI.dismiss();
      }
    } catch (ex) {}
  },

  shutdown: function shutdown() {
    this.flushActiveTileset();
  },

  /*
   * Called from various places when the visible content
   * has changed such that button states may need to be
   * updated.
   */
  update: function update() {
    this._updatePinButton();
    this._updateStarButton();
  },

  onPinButton: function() {
    if (this.pinButton.checked) {
      Browser.pinSite();
    } else {
      Browser.unpinSite();
    }
  },

  onStarButton: function(aValue) {
    if (aValue === undefined) {
      aValue = this.starButton.checked;
    }

    if (aValue) {
      Browser.starSite(function () {
        Appbar._updateStarButton();
      });
    } else {
      Browser.unstarSite(function () {
        Appbar._updateStarButton();
      });
    }
  },

  onMenuButton: function(aEvent) {
      let typesArray = [];

      if (BrowserUI.isPrivateBrowsingEnabled) {
        typesArray.push("private-browsing");
      }
      if (!BrowserUI.isStartTabVisible) {
        typesArray.push("find-in-page");
        if (ContextCommands.getPageSource())
          typesArray.push("view-page-source");
      }
      if (ContextCommands.getStoreLink())
        typesArray.push("ms-meta-data");
      if (ConsolePanelView.enabled)
        typesArray.push("open-error-console");
      if (!Services.metro.immersive)
        typesArray.push("open-jsshell");

      typesArray.push("view-on-desktop");

      var x = this.menuButton.getBoundingClientRect().left;
      var y = Elements.toolbar.getBoundingClientRect().top;
      ContextMenuUI.showContextMenu({
        json: {
          types: typesArray,
          string: '',
          xPos: x,
          yPos: y,
          leftAligned: true,
          bottomAligned: true
      }

      });
  },

  onViewOnDesktop: function() {
    let appStartup = Components.classes["@mozilla.org/toolkit/app-startup;1"].
      getService(Components.interfaces.nsIAppStartup);

    Services.prefs.setBoolPref('browser.sessionstore.resume_session_once', true);
    this._incrementCountableEvent("switch-to-desktop-button");
    appStartup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit |
                    Components.interfaces.nsIAppStartup.eRestart);
  },

  onAutocompleteCloseButton: function () {
    Elements.autocomplete.closePopup();
  },

  dispatchContextualAction: function(aActionName){
    let activeTileset = this.activeTileset;
    if (activeTileset && ('isBound' in this.activeTileset)) {
      // fire event on the richgrid, others can listen
      // but we keep coupling loose so grid doesn't need to know about appbar
      let event = document.createEvent("Events");
      event.action = aActionName;
      event.initEvent("context-action", true, true); // is cancelable
      activeTileset.dispatchEvent(event);
      if (!event.defaultPrevented) {
        activeTileset.clearSelection();
        Elements.contextappbar.dismiss();
      }
    }
  },

  showContextualActions: function(aVerbs, aSetName) {
    // When the appbar is not visible, we want the icons to refresh right away
    let immediate = !Elements.contextappbar.isShowing;

    if (aVerbs.length) {
      Elements.contextappbar.show();
    }

    // Look up all of the buttons for the verbs that should be visible.
    let idsToVisibleVerbs = new Map();
    for (let verb of aVerbs) {
      let id = verb + "-selected-button";
      if (!document.getElementById(id)) {
        throw new Error("Appbar.showContextualActions: no button for " + verb);
      }
      idsToVisibleVerbs.set(id, verb);
    }

    // Sort buttons into 2 buckets - needing showing and needing hiding.
    let toHide = [], toShow = [];
    let buttons = Elements.contextappbar.getElementsByTagName("toolbarbutton");
    for (let button of buttons) {
      let verb = idsToVisibleVerbs.get(button.id);
      if (verb != undefined) {
        // Button should be visible, and may or may not be showing.
        this._updateContextualActionLabel(button, verb, aSetName);
        if (button.hidden) {
          toShow.push(button);
        }
      } else if (!button.hidden) {
        // Button is visible, but shouldn't be.
        toHide.push(button);
      }
    }

    if (immediate) {
      toShow.forEach(function(element) {
        element.removeAttribute("fade");
        element.hidden = false;
      });
      toHide.forEach(function(element) {
        element.setAttribute("fade", true);
        element.hidden = true;
      });
      return;
    }

    return Task.spawn(function() {
      if (toHide.length) {
        yield Util.transitionElementVisibility(toHide, false);
      }
      if (toShow.length) {
        yield Util.transitionElementVisibility(toShow, true);
      }
    });
  },

  _clearContextualActions: function() {
    this.showContextualActions([]);
  },

  _updateContextualActionLabel: function(aButton, aVerb, aSetName) {
    // True if the action's label string contains the set name and
    // thus has to be selected based on the list passed in.
    let usesSetName = aButton.hasAttribute("label-uses-set-name");
    let name = "contextAppbar2." + aVerb + (usesSetName ? "." + aSetName : "");
    aButton.label = Strings.browser.GetStringFromName(name);
  },

  _onTileSelectionChanged: function _onTileSelectionChanged(aEvent){
    let activeTileset = aEvent.target;

    // deselect tiles in other tile groups,
    // ensure previousyl-activeTileset is bound before calling methods on it
    if (this.activeTileset &&
          ('isBound' in this.activeTileset) &&
          this.activeTileset !== activeTileset) {
      this.activeTileset.clearSelection();
    }
    // keep track of which view is the target/scope for the contextual actions
    this.activeTileset = activeTileset;

    // ask the view for the list verbs/action-names it thinks are
    // appropriate for the tiles selected
    let contextActions = activeTileset.contextActions;
    let verbs = [v for (v of contextActions)];

    // fire event with these verbs as payload
    let event = document.createEvent("Events");
    event.actions = verbs;
    event.initEvent("MozContextActionsChange", true, false);
    activeTileset.dispatchEvent(event);

    if (verbs.length) {
      Elements.contextappbar.show(); // should be no-op if we're already showing
    } else {
      Elements.contextappbar.dismiss();
    }
  },

  // track certain appbar events and interactions for the UITelemetry probe
  _countableEvents: {},

  _incrementCountableEvent: function(aName) {
    if (!(aName in this._countableEvents)) {
      this._countableEvents[aName] = 0;
    }
    this._countableEvents[aName]++;
  },

  getAppbarMeasures: function() {
    return {
      countableEvents: this._countableEvents
    };
  },

  _updatePinButton: function() {
    this.pinButton.checked = Browser.isSitePinned();
  },

  _updateStarButton: function() {
    Browser.isSiteStarredAsync(function (isStarred) {
      this.starButton.checked = isStarred;
    }.bind(this));
  },

};
