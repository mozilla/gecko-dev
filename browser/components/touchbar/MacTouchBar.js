/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gTouchBarUpdater",
  "@mozilla.org/widget/touchbarupdater;1",
  "nsITouchBarUpdater"
);

// For accessing TouchBarHelper methods from static contexts in this file.
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gTouchBarHelper",
  "@mozilla.org/widget/touchbarhelper;1",
  "nsITouchBarHelper"
);

/**
 * Executes a XUL command on the top window. Called by the callbacks in each
 * TouchBarInput.
 * @param {string} commandName
 *        A XUL command.
 * @param {string} telemetryKey
 *        A string describing the command, sent for telemetry purposes.
 *        Intended to be shorter and more readable than the XUL command.
 */
function execCommand(commandName, telemetryKey) {
  let command = TouchBarHelper.window.document.getElementById(commandName);
  if (command) {
    command.doCommand();
  }
  let telemetry = Services.telemetry.getHistogramById(
    "TOUCHBAR_BUTTON_PRESSES"
  );
  telemetry.add(telemetryKey);
}

/**
 * Static helper function to convert a hexadecimal string to its integer
 * value. Used to convert colours to a format accepted by Apple's NSColor code.
 * @param {string} hexString
 *        A hexadecimal string, optionally beginning with '#'.
 */
function hexToInt(hexString) {
  if (!hexString) {
    return null;
  }
  if (hexString.charAt(0) == "#") {
    hexString = hexString.slice(1);
  }
  let val = parseInt(hexString, 16);
  return isNaN(val) ? null : val;
}

const kInputTypes = {
  BUTTON: "button",
  LABEL: "label",
  MAIN_BUTTON: "mainButton",
  POPOVER: "popover",
  SCROLLVIEW: "scrollView",
  SCRUBBER: "scrubber",
};

/**
 * An object containing all implemented TouchBarInput objects.
 */
const kBuiltInInputs = {
  Back: {
    title: "back",
    image: "chrome://browser/skin/back.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("Browser:Back", "Back"),
  },
  Forward: {
    title: "forward",
    image: "chrome://browser/skin/forward.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("Browser:Forward", "Forward"),
  },
  Reload: {
    title: "reload",
    image: "chrome://browser/skin/reload.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("Browser:Reload", "Reload"),
  },
  Home: {
    title: "home",
    image: "chrome://browser/skin/home.svg",
    type: kInputTypes.BUTTON,
    callback: () => {
      let win = BrowserWindowTracker.getTopWindow();
      win.BrowserHome();
      let telemetry = Services.telemetry.getHistogramById(
        "TOUCHBAR_BUTTON_PRESSES"
      );
      telemetry.add("Home");
    },
  },
  Fullscreen: {
    title: "fullscreen",
    image: "chrome://browser/skin/fullscreen.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("View:FullScreen", "Fullscreen"),
  },
  Find: {
    title: "find",
    image: "chrome://browser/skin/search-glass.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("cmd_find", "Find"),
  },
  NewTab: {
    title: "new-tab",
    image: "chrome://browser/skin/add.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("cmd_newNavigatorTabNoEvent", "NewTab"),
  },
  Sidebar: {
    title: "open-sidebar",
    image: "chrome://browser/skin/sidebars.svg",
    type: kInputTypes.BUTTON,
    callback: () => {
      let win = BrowserWindowTracker.getTopWindow();
      win.SidebarUI.toggle();
      let telemetry = Services.telemetry.getHistogramById(
        "TOUCHBAR_BUTTON_PRESSES"
      );
      telemetry.add("Sidebar");
    },
  },
  AddBookmark: {
    title: "add-bookmark",
    image: "chrome://browser/skin/bookmark-hollow.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("Browser:AddBookmarkAs", "AddBookmark"),
  },
  ReaderView: {
    title: "reader-view",
    image: "chrome://browser/skin/readerMode.svg",
    type: kInputTypes.BUTTON,
    callback: () => execCommand("View:ReaderView", "ReaderView"),
    disabled: true, // Updated when the page is found to be Reader View-able.
  },
  OpenLocation: {
    title: "open-location",
    image: "chrome://browser/skin/search-glass.svg",
    type: kInputTypes.MAIN_BUTTON,
    callback: () => execCommand("Browser:OpenLocation", "OpenLocation"),
  },
  // This is a special-case `type: kInputTypes.SCRUBBER` element.
  // Scrubbers are not yet generally implemented.
  // See follow-up bug 1502539.
  Share: {
    title: "share",
    image: "chrome://browser/skin/share.svg",
    type: kInputTypes.SCRUBBER,
    callback: () => execCommand("cmd_share", "Share"),
  },
  SearchPopover: {
    title: "search-popover",
    image: "chrome://browser/skin/search-glass.svg",
    type: kInputTypes.POPOVER,
    children: {
      SearchScrollViewLabel: {
        title: "search-search-in",
        type: kInputTypes.LABEL,
      },
      SearchScrollView: {
        key: "search-scrollview",
        type: kInputTypes.SCROLLVIEW,
        children: {
          Bookmarks: {
            title: "search-bookmarks",
            type: kInputTypes.BUTTON,
            callback: () =>
              gTouchBarHelper.insertRestrictionInUrlbar(
                UrlbarTokenizer.RESTRICT.BOOKMARK
              ),
          },
          History: {
            title: "search-history",
            type: kInputTypes.BUTTON,
            callback: () =>
              gTouchBarHelper.insertRestrictionInUrlbar(
                UrlbarTokenizer.RESTRICT.HISTORY
              ),
          },
          OpenTabs: {
            title: "search-opentabs",
            type: kInputTypes.BUTTON,
            callback: () =>
              gTouchBarHelper.insertRestrictionInUrlbar(
                UrlbarTokenizer.RESTRICT.OPENPAGE
              ),
          },
          Tags: {
            title: "search-tags",
            type: kInputTypes.BUTTON,
            callback: () =>
              gTouchBarHelper.insertRestrictionInUrlbar(
                UrlbarTokenizer.RESTRICT.TAG
              ),
          },
          Titles: {
            title: "search-titles",
            type: kInputTypes.BUTTON,
            callback: () =>
              gTouchBarHelper.insertRestrictionInUrlbar(
                UrlbarTokenizer.RESTRICT.TITLE
              ),
          },
        },
      },
    },
  },
};

const kHelperObservers = new Set([
  "bookmark-icon-updated",
  "reader-mode-available",
  "touchbar-location-change",
  "quit-application",
  "intl:app-locales-changed",
  "urlbar-focus",
  "urlbar-blur",
]);

/**
 * JS-implemented TouchBarHelper class.
 * Provides services to the Mac Touch Bar.
 */
class TouchBarHelper {
  constructor() {
    for (let topic of kHelperObservers) {
      Services.obs.addObserver(this, topic);
    }
    // We cache our search popover since otherwise it is frequently
    // created/destroyed for the urlbar-focus/blur events.
    this._searchPopover = this.getTouchBarInput("SearchPopover");
  }

  destructor() {
    this._searchPopover = null;
    for (let topic of kHelperObservers) {
      Services.obs.removeObserver(this, topic);
    }
  }

  get activeTitle() {
    let tabbrowser = TouchBarHelper.window.ownerGlobal.gBrowser;
    let activeTitle;
    if (tabbrowser) {
      activeTitle = tabbrowser.selectedBrowser.contentTitle;
    }
    return activeTitle;
  }

  get allItems() {
    let layoutItems = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );

    for (let inputName of Object.keys(kBuiltInInputs)) {
      if (typeof kBuiltInInputs[inputName].context == "function") {
        inputName = kBuiltInInputs[inputName].context();
      }
      let input = this.getTouchBarInput(inputName);
      if (!input) {
        continue;
      }
      layoutItems.appendElement(input);
    }

    return layoutItems;
  }

  static get window() {
    return BrowserWindowTracker.getTopWindow();
  }

  get isUrlbarFocused() {
    if (!TouchBarHelper.window || !TouchBarHelper.window.gURLBar) {
      return false;
    }
    return TouchBarHelper.window.gURLBar.focused;
  }

  static get baseWindow() {
    return TouchBarHelper.window.docShell.treeOwner.QueryInterface(
      Ci.nsIBaseWindow
    );
  }

  getTouchBarInput(inputName) {
    if (inputName == "SearchPopover" && this._searchPopover) {
      return this._searchPopover;
    }

    // inputName might be undefined if an input's context() returns undefined.
    if (!inputName || !kBuiltInInputs.hasOwnProperty(inputName)) {
      return null;
    }

    // context() may specify that one named input "point" to another.
    if (typeof kBuiltInInputs[inputName].context == "function") {
      inputName = kBuiltInInputs[inputName].context();
    }

    if (!inputName || !kBuiltInInputs.hasOwnProperty(inputName)) {
      return null;
    }

    let inputData = kBuiltInInputs[inputName];

    let item = new TouchBarInput(inputData);

    // Skip localization if there is already a cached localized title or if
    // no title is needed.
    if (
      kBuiltInInputs[inputName].hasOwnProperty("localTitle") ||
      !kBuiltInInputs[inputName].hasOwnProperty("title")
    ) {
      return item;
    }

    // Async l10n fills in the localized input labels after the initial load.
    this._l10n.formatValue(item.key).then(result => {
      item.title = result;
      kBuiltInInputs[inputName].localTitle = result; // Cache result.
      // Checking TouchBarHelper.window since this callback can fire after all windows are closed.
      if (TouchBarHelper.window) {
        gTouchBarUpdater.updateTouchBarInputs(TouchBarHelper.baseWindow, [
          item,
        ]);
      }
    });

    return item;
  }

  /**
   * Fetches a specific Touch Bar Input by name and updates it on the Touch Bar.
   * @param {string} inputName
   *        A key to a value in the kBuiltInInputs object in this file.
   * @param {...*} [otherInputs] (optional)
   *        Additional keys to values in the kBuiltInInputs object in this file.
   */
  _updateTouchBarInputs(...inputNames) {
    if (!TouchBarHelper.window) {
      return;
    }

    let inputs = [];
    for (let inputName of inputNames) {
      let input = this.getTouchBarInput(inputName);
      if (!input) {
        continue;
      }
      inputs.push(input);
    }

    gTouchBarUpdater.updateTouchBarInputs(TouchBarHelper.baseWindow, inputs);
  }

  /**
   * Inserts a restriction token into the Urlbar ahead of the current typed
   * search term.
   * @param {string} restrictionToken
   *        The restriction token to be inserted into the Urlbar. Preferably
   *        sourced from UrlbarTokenizer.RESTRICT.
   */
  insertRestrictionInUrlbar(restrictionToken) {
    let searchString = TouchBarHelper.window.gURLBar.lastSearchString.trimStart();
    if (Object.values(UrlbarTokenizer.RESTRICT).includes(searchString[0])) {
      searchString = searchString.substring(1).trimStart();
    }

    TouchBarHelper.window.gURLBar.search(`${restrictionToken} ${searchString}`);
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "touchbar-location-change":
        this.activeUrl = data;
        // ReaderView button is disabled on every location change since
        // Reader View must determine if the new page can be Reader Viewed.
        kBuiltInInputs.ReaderView.disabled = !data.startsWith("about:reader");
        kBuiltInInputs.Back.disabled = !TouchBarHelper.window.gBrowser
          .canGoBack;
        kBuiltInInputs.Forward.disabled = !TouchBarHelper.window.gBrowser
          .canGoForward;
        this._updateTouchBarInputs("ReaderView", "Back", "Forward");
        break;
      case "bookmark-icon-updated":
        data == "starred"
          ? (kBuiltInInputs.AddBookmark.image =
              "chrome://browser/skin/bookmark.svg")
          : (kBuiltInInputs.AddBookmark.image =
              "chrome://browser/skin/bookmark-hollow.svg");
        this._updateTouchBarInputs("AddBookmark");
        break;
      case "reader-mode-available":
        kBuiltInInputs.ReaderView.disabled = false;
        this._updateTouchBarInputs("ReaderView");
        break;
      case "urlbar-focus":
        if (!this._searchPopover) {
          this._searchPopover = this.getTouchBarInput("SearchPopover");
        }
        gTouchBarUpdater.showPopover(
          TouchBarHelper.baseWindow,
          this._searchPopover,
          true
        );
        break;
      case "urlbar-blur":
        if (!this._searchPopover) {
          this._searchPopover = this.getTouchBarInput("SearchPopover");
        }
        gTouchBarUpdater.showPopover(
          TouchBarHelper.baseWindow,
          this._searchPopover,
          false
        );
        break;
      case "intl:app-locales-changed":
        // On locale change, refresh all inputs after loading new localTitle.
        this._searchPopover = null;
        for (let input in kBuiltInInputs) {
          delete input.localTitle;
        }
        this._updateTouchBarInputs(...kBuiltInInputs.keys());
        break;
      case "quit-application":
        this.destructor();
        break;
    }
  }
}

const helperProto = TouchBarHelper.prototype;
helperProto.classDescription = "Services the Mac Touch Bar";
helperProto.classID = Components.ID("{ea109912-3acc-48de-b679-c23b6a122da5}");
helperProto.contractID = "@mozilla.org/widget/touchbarhelper;1";
helperProto.QueryInterface = ChromeUtils.generateQI([Ci.nsITouchBarHelper]);
helperProto._l10n = new Localization(["browser/touchbar/touchbar.ftl"]);

/**
 * A representation of a Touch Bar input.
 *     @param {object} input
 *            An object representing a Touch Bar Input.
 *            Contains listed properties.
 *     @param {string} input.title
 *            The lookup key for the button's localized text title.
 *     @param {string} input.image
 *            The name of a icon file added to
 *            /widget/cocoa/resources/touchbar_icons.
 *     @param {string} input.type
 *            The type of Touch Bar input represented by the object.
 *            One of `button`, `mainButton`.
 *     @param {Function} input.callback
 *            A callback invoked when a touchbar item is touched.
 *     @param {string} [input.color]
 *            A string in hex format specifying the button's background color.
 *            If omitted, the default background color is used.
 *     @param {bool} [input.disabled]
 *            If `true`, the Touch Bar input is greyed out and inoperable.
 *     @param {Array} [input.children]
 *            An array of input objects that will be displayed as children of
 *            this input. Available only for types KInputTypes.POPOVER and
 *            kInputTypes.SCROLLVIEW.
 */
class TouchBarInput {
  constructor(input) {
    this._key = input.key || input.title;
    this._title = input.hasOwnProperty("localTitle") ? input.localTitle : "";
    this._image = input.image;
    this._type = input.type;
    this._callback = input.callback;
    this._color = hexToInt(input.color);
    this._disabled = input.hasOwnProperty("disabled") ? input.disabled : false;
    if (input.children) {
      this._children = [];
      let toLocalize = [];
      for (let childData of Object.values(input.children)) {
        let initializedChild = new TouchBarInput(childData);
        if (!initializedChild) {
          continue;
        }
        // Children's types are prepended by the parent's type. This is so we
        // can uniquely identify a child input from a standalone input with
        // the same name. (e.g. a button called "back" in a popover would be a
        // "popover-button.back" vs. a "button.back").
        initializedChild.type = input.type + "-" + initializedChild.type;
        this._children.push(initializedChild);
        // Skip l10n for inputs without a title or those already localized.
        if (childData.title && childData.title != "") {
          toLocalize.push(initializedChild);
        }
      }
      this._localizeChildren(toLocalize);
    }
  }

  get key() {
    return this._key;
  }
  get title() {
    return this._title;
  }
  set title(title) {
    this._title = title;
  }
  get image() {
    return this._image ? PlacesUtils.toURI(this._image) : null;
  }
  set image(image) {
    this._image = image;
  }
  // Required as context to load our input icons.
  get document() {
    return BrowserWindowTracker.getTopWindow().document;
  }
  get type() {
    return this._type == "" ? "button" : this._type;
  }
  set type(type) {
    this._type = type;
  }
  get callback() {
    return this._callback;
  }
  set callback(callback) {
    this._callback = callback;
  }
  get color() {
    return this._color;
  }
  set color(color) {
    this._color = this.hexToInt(color);
  }
  get disabled() {
    return this._disabled || false;
  }
  set disabled(disabled) {
    this._disabled = disabled;
  }
  get children() {
    if (!this._children) {
      return null;
    }
    let children = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );
    for (let child of this._children) {
      children.appendElement(child);
    }
    return children;
  }

  /**
   * Apply Fluent l10n to child inputs.
   * @param {Array} children An array of initialized TouchBarInputs.
   */
  async _localizeChildren(children) {
    let titles = await helperProto._l10n.formatValues(
      children.map(child => ({ id: child.key }))
    );
    // In the TouchBarInput constuctor, we filtered so children contains only
    // those inputs with titles to be localized. We can be confident that the
    // results in titles match up with the inputs to be localized.
    children.forEach(function(child, index) {
      child.title = titles[index];
    });
    gTouchBarUpdater.updateTouchBarInputs(TouchBarHelper.baseWindow, children);
  }
}

const inputProto = TouchBarInput.prototype;
inputProto.classDescription = "Represents an input on the Mac Touch Bar";
inputProto.classID = Components.ID("{77441d17-f29c-49d7-982f-f20a5ab5a900}");
inputProto.contractID = "@mozilla.org/widget/touchbarinput;1";
inputProto.QueryInterface = ChromeUtils.generateQI([Ci.nsITouchBarInput]);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  TouchBarHelper,
  TouchBarInput,
]);
