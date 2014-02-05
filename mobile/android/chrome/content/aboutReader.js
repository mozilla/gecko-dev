/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function ()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

function dump(s) {
  Services.console.logStringMessage("AboutReader: " + s);
}

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutReader.properties");

let AboutReader = function(doc, win) {
  dump("Init()");

  this._docRef = Cu.getWeakReference(doc);
  this._winRef = Cu.getWeakReference(win);

  Services.obs.addObserver(this, "Reader:FaviconReturn", false);
  Services.obs.addObserver(this, "Reader:Add", false);
  Services.obs.addObserver(this, "Reader:Remove", false);
  Services.obs.addObserver(this, "Reader:ListCountReturn", false);
  Services.obs.addObserver(this, "Reader:ListCountUpdated", false);
  Services.obs.addObserver(this, "Reader:ListStatusReturn", false);

  this._article = null;

  dump("Feching toolbar, header and content notes from about:reader");
  this._headerElementRef = Cu.getWeakReference(doc.getElementById("reader-header"));
  this._domainElementRef = Cu.getWeakReference(doc.getElementById("reader-domain"));
  this._titleElementRef = Cu.getWeakReference(doc.getElementById("reader-title"));
  this._creditsElementRef = Cu.getWeakReference(doc.getElementById("reader-credits"));
  this._contentElementRef = Cu.getWeakReference(doc.getElementById("reader-content"));
  this._toolbarElementRef = Cu.getWeakReference(doc.getElementById("reader-toolbar"));
  this._messageElementRef = Cu.getWeakReference(doc.getElementById("reader-message"));

  this._toolbarEnabled = false;

  this._scrollOffset = win.pageYOffset;

  let body = doc.body;
  body.addEventListener("touchstart", this, false);
  body.addEventListener("click", this, false);

  win.addEventListener("unload", this, false);
  win.addEventListener("scroll", this, false);
  win.addEventListener("popstate", this, false);
  win.addEventListener("resize", this, false);

  this._setupAllDropdowns();
  this._setupButton("toggle-button", this._onReaderToggle.bind(this));
  this._setupButton("list-button", this._onList.bind(this));
  this._setupButton("share-button", this._onShare.bind(this));

  let colorSchemeOptions = [
    { name: gStrings.GetStringFromName("aboutReader.colorSchemeDark"),
      value: "dark"},
    { name: gStrings.GetStringFromName("aboutReader.colorSchemeLight"),
      value: "light"},
    { name: gStrings.GetStringFromName("aboutReader.colorSchemeAuto"),
      value: "auto"}
  ];

  let colorScheme = Services.prefs.getCharPref("reader.color_scheme");
  this._setupSegmentedButton("color-scheme-buttons", colorSchemeOptions, colorScheme, this._setColorSchemePref.bind(this));
  this._setColorSchemePref(colorScheme);

  let fontTypeSample = gStrings.GetStringFromName("aboutReader.fontTypeSample");
  let fontTypeOptions = [
    { name: fontTypeSample,
      description: gStrings.GetStringFromName("aboutReader.fontTypeSerif"),
      value: "serif",
      linkClass: "serif" },
    { name: fontTypeSample,
      description: gStrings.GetStringFromName("aboutReader.fontTypeSansSerif"),
      value: "sans-serif",
      linkClass: "sans-serif"
    },
  ];

  let fontType = Services.prefs.getCharPref("reader.font_type");
  this._setupSegmentedButton("font-type-buttons", fontTypeOptions, fontType, this._setFontType.bind(this));
  this._setFontType(fontType);

  let fontSizeSample = gStrings.GetStringFromName("aboutReader.fontSizeSample");
  let fontSizeOptions = [
    { name: fontSizeSample,
      value: 1,
      linkClass: "font-size1-sample" },
    { name: fontSizeSample,
      value: 2,
      linkClass: "font-size2-sample" },
    { name: fontSizeSample,
      value: 3,
      linkClass: "font-size3-sample" },
    { name: fontSizeSample,
      value: 4,
      linkClass: "font-size4-sample" },
    { name: fontSizeSample,
      value: 5,
      linkClass: "font-size5-sample" }
  ];

  let fontSize = Services.prefs.getIntPref("reader.font_size");
  this._setupSegmentedButton("font-size-buttons", fontSizeOptions, fontSize, this._setFontSize.bind(this));
  this._setFontSize(fontSize);

  dump("Decoding query arguments");
  let queryArgs = this._decodeQueryString(win.location.href);

  // Track status of reader toolbar add/remove toggle button
  this._isReadingListItem = -1;
  this._updateToggleButton();

  // Track status of reader toolbar list button
  this._readingListCount = -1;
  this._updateListButton();
  this._requestReadingListCount();

  let url = queryArgs.url;
  let tabId = queryArgs.tabId;
  if (tabId) {
    dump("Loading from tab with ID: " + tabId + ", URL: " + url);
    this._loadFromTab(tabId, url);
  } else {
    dump("Fetching page with URL: " + url);
    this._loadFromURL(url);
  }
}

AboutReader.prototype = {
  _BLOCK_IMAGES_SELECTOR: ".content p > img:only-child, " +
                          ".content p > a:only-child > img:only-child, " +
                          ".content .wp-caption img, " +
                          ".content figure img",

  get _doc() {
    return this._docRef.get();
  },

  get _win() {
    return this._winRef.get();
  },

  get _headerElement() {
    return this._headerElementRef.get();
  },

  get _domainElement() {
    return this._domainElementRef.get();
  },

  get _titleElement() {
    return this._titleElementRef.get();
  },

  get _creditsElement() {
    return this._creditsElementRef.get();
  },

  get _contentElement() {
    return this._contentElementRef.get();
  },

  get _toolbarElement() {
    return this._toolbarElementRef.get();
  },

  get _messageElement() {
    return this._messageElementRef.get();
  },

  observe: function Reader_observe(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "Reader:FaviconReturn": {
        let args = JSON.parse(aData);
        this._loadFavicon(args.url, args.faviconUrl);
        Services.obs.removeObserver(this, "Reader:FaviconReturn");
        break;
      }

      case "Reader:Add": {
        let args = JSON.parse(aData);
        if (args.url == this._article.url) {
          if (this._isReadingListItem != 1) {
            this._isReadingListItem = 1;
            this._updateToggleButton();
          }
        }
        break;
      }

      case "Reader:Remove": {
        if (aData == this._article.url) {
          if (this._isReadingListItem != 0) {
            this._isReadingListItem = 0;
            this._updateToggleButton();
          }
        }
        break;
      }

      case "Reader:ListCountReturn":
      case "Reader:ListCountUpdated": {
        let count = parseInt(aData);
        if (this._readingListCount != count) {
          let isInitialStateChange = (this._readingListCount == -1);
          this._readingListCount = count;
          this._updateListButton();

          // Display the toolbar when all its initial component states are known
          if (isInitialStateChange) {
            this._setToolbarVisibility(true);
          }

          // Initial readinglist count is requested before any page is displayed
          if (this._article) {
            this._requestReadingListStatus();
          }
        }
        break;
      }

      case "Reader:ListStatusReturn": {
        let args = JSON.parse(aData);
        if (args.url == this._article.url) {
          if (this._isReadingListItem != args.inReadingList) {
            let isInitialStateChange = (this._isReadingListItem == -1);
            this._isReadingListItem = args.inReadingList;
            this._updateToggleButton();

            // Display the toolbar when all its initial component states are known
            if (isInitialStateChange) {
              this._setToolbarVisibility(true);
            }
          }
        }
        break;
      }
    }
  },

  handleEvent: function Reader_handleEvent(aEvent) {
    if (!aEvent.isTrusted)
      return;

    switch (aEvent.type) {
      case "touchstart":
        this._scrolled = false;
        break;
      case "click":
        if (!this._scrolled)
          this._toggleToolbarVisibility();
        break;
      case "scroll":
        if (!this._scrolled) {
          let isScrollingUp = this._scrollOffset > aEvent.pageY;
          this._setToolbarVisibility(isScrollingUp);
          this._scrollOffset = aEvent.pageY;
        }
        break;
      case "popstate":
        if (!aEvent.state)
          this._closeAllDropdowns();
        break;
      case "resize":
        this._updateImageMargins();
        break;

      case "devicelight":
        this._handleDeviceLight(aEvent.value);
        break;

      case "unload":
        Services.obs.removeObserver(this, "Reader:Add");
        Services.obs.removeObserver(this, "Reader:Remove");
        Services.obs.removeObserver(this, "Reader:ListCountReturn");
        Services.obs.removeObserver(this, "Reader:ListCountUpdated");
        Services.obs.removeObserver(this, "Reader:ListStatusReturn");
        break;
    }
  },

  _updateToggleButton: function Reader_updateToggleButton() {
    let classes = this._doc.getElementById("toggle-button").classList;

    if (this._isReadingListItem == 1) {
      classes.add("on");
    } else {
      classes.remove("on");
    }
  },

  _updateListButton: function Reader_updateListButton() {
    let classes = this._doc.getElementById("list-button").classList;

    if (this._readingListCount > 0) {
      classes.add("on");
    } else {
      classes.remove("on");
    }
  },

  _requestReadingListCount: function Reader_requestReadingListCount() {
    gChromeWin.sendMessageToJava({ type: "Reader:ListCountRequest" });
  },

  _requestReadingListStatus: function Reader_requestReadingListStatus() {
    gChromeWin.sendMessageToJava({
      type: "Reader:ListStatusRequest",
      url: this._article.url
    });
  },

  _onReaderToggle: function Reader_onToggle() {
    if (!this._article)
      return;

    this._isReadingListItem = (this._isReadingListItem == 1) ? 0 : 1;
    this._updateToggleButton();

    if (this._isReadingListItem == 1) {
      gChromeWin.Reader.storeArticleInCache(this._article, function(success) {
        dump("Reader:Add (in reader) success=" + success);

        let result = (success ? gChromeWin.Reader.READER_ADD_SUCCESS :
            gChromeWin.Reader.READER_ADD_FAILED);

        let json = JSON.stringify({ fromAboutReader: true, url: this._article.url });
        Services.obs.notifyObservers(null, "Reader:Add", json);

        gChromeWin.sendMessageToJava({
          type: "Reader:Added",
          result: result,
          title: this._article.title,
          url: this._article.url,
        });
      }.bind(this));
    } else {
      gChromeWin.Reader.removeArticleFromCache(this._article.url , function(success) {
        dump("Reader:Remove (in reader) success=" + success);

        Services.obs.notifyObservers(null, "Reader:Remove", this._article.url);

        gChromeWin.sendMessageToJava({
          type: "Reader:Removed",
          url: this._article.url
        });
      }.bind(this));
    }
  },

  _onList: function Reader_onList() {
    if (!this._article || this._readingListCount < 1)
      return;

    gChromeWin.BrowserApp.loadURI("about:home?page=reading_list");
  },

  _onShare: function Reader_onShare() {
    if (!this._article)
      return;

    gChromeWin.sendMessageToJava({
      type: "Reader:Share",
      url: this._article.url,
      title: this._article.title
    });
  },

  _setFontSize: function Reader_setFontSize(newFontSize) {
    let bodyClasses = this._doc.body.classList;

    if (this._fontSize > 0)
      bodyClasses.remove("font-size" + this._fontSize);

    this._fontSize = newFontSize;
    bodyClasses.add("font-size" + this._fontSize);

    Services.prefs.setIntPref("reader.font_size", this._fontSize);
  },

  _handleDeviceLight: function Reader_handleDeviceLight(newLux) {
    // Desired size of the this._luxValues array.
    let luxValuesSize = 10;
    // Add new lux value at the front of the array.
    this._luxValues.unshift(newLux);
    // Add new lux value to this._totalLux for averaging later.
    this._totalLux += newLux;

    // Don't update when length of array is less than luxValuesSize except when it is 1.
    if (this._luxValues.length < luxValuesSize) {
      // Use the first lux value to set the color scheme until our array equals luxValuesSize.
      if (this._luxValues.length == 1) {
        this._updateColorScheme(newLux);
      }
      return;
    }
    // Holds the average of the lux values collected in this._luxValues.
    let averageLuxValue = this._totalLux/luxValuesSize;

    this._updateColorScheme(averageLuxValue);
    // Pop the oldest value off the array.
    let oldLux = this._luxValues.pop();
    // Subtract oldLux since it has been discarded from the array.
    this._totalLux -= oldLux;
  },

  _updateColorScheme: function Reader_updateColorScheme(luxValue) {
    // Upper bound value for "dark" color scheme beyond which it changes to "light".
    let upperBoundDark = 50;
    // Lower bound value for "light" color scheme beyond which it changes to "dark".
    let lowerBoundLight = 10;
    // Threshold for color scheme change.
    let colorChangeThreshold = 20;

    // Ignore changes that are within a certain threshold of previous lux values.
    if ((this._colorScheme === "dark" && luxValue < upperBoundDark) ||
        (this._colorScheme === "light" && luxValue > lowerBoundLight))
      return;

    if (luxValue < colorChangeThreshold)
      this._setColorScheme("dark");
    else
      this._setColorScheme("light");
  },

  _setColorScheme: function Reader_setColorScheme(newColorScheme) {
    if (this._colorScheme === newColorScheme)
      return;

    let bodyClasses = this._doc.body.classList;

    if (this._colorScheme)
      bodyClasses.remove(this._colorScheme);

    this._colorScheme = newColorScheme;
    bodyClasses.add(this._colorScheme);
  },

  // Pref values include "dark", "light", and "auto", which automatically switches
  // between light and dark color schemes based on the ambient light level.
  _setColorSchemePref: function Reader_setColorSchemePref(colorSchemePref) {
    if (colorSchemePref === "auto") {
      this._win.addEventListener("devicelight", this, false);
      this._luxValues = [];
      this._totalLux = 0;
    } else {
      this._win.removeEventListener("devicelight", this, false);
      this._setColorScheme(colorSchemePref);
      delete this._luxValues;
      delete this._totalLux;
    }

    Services.prefs.setCharPref("reader.color_scheme", colorSchemePref);
  },

  _setFontType: function Reader_setFontType(newFontType) {
    if (this._fontType === newFontType)
      return;

    let bodyClasses = this._doc.body.classList;

    if (this._fontType)
      bodyClasses.remove(this._fontType);

    this._fontType = newFontType;
    bodyClasses.add(this._fontType);

    Services.prefs.setCharPref("reader.font_type", this._fontType);
  },

  _getToolbarVisibility: function Reader_getToolbarVisibility() {
    return !this._toolbarElement.classList.contains("toolbar-hidden");
  },

  _setToolbarVisibility: function Reader_setToolbarVisibility(visible) {
    let win = this._win;
    if (win.history.state)
      win.history.back();

    if (!this._toolbarEnabled)
      return;

    // Don't allow visible toolbar until banner state is known
    if (this._readingListCount == -1 || this._isReadingListItem == -1)
      return;

    if (this._getToolbarVisibility() === visible)
      return;

    this._toolbarElement.classList.toggle("toolbar-hidden");
    this._setSystemUIVisibility(visible);

    if (!visible && !this._hasUsedToolbar) {
      this._hasUsedToolbar = Services.prefs.getBoolPref("reader.has_used_toolbar");
      if (!this._hasUsedToolbar) {
        gChromeWin.NativeWindow.toast.show(gStrings.GetStringFromName("aboutReader.toolbarTip"), "short");

        Services.prefs.setBoolPref("reader.has_used_toolbar", true);
        this._hasUsedToolbar = true;
      }
    }
  },

  _toggleToolbarVisibility: function Reader_toggleToolbarVisibility(visible) {
    this._setToolbarVisibility(!this._getToolbarVisibility());
  },

  _setSystemUIVisibility: function Reader_setSystemUIVisibility(visible) {
    gChromeWin.sendMessageToJava({
      type: "SystemUI:Visibility",
      visible: visible
    });
  },

  _loadFromURL: function Reader_loadFromURL(url) {
    this._showProgressDelayed();

    gChromeWin.Reader.parseDocumentFromURL(url, function(article) {
      if (article)
        this._showContent(article);
      else
        this._showError(gStrings.GetStringFromName("aboutReader.loadError"));
    }.bind(this));
  },

  _loadFromTab: function Reader_loadFromTab(tabId, url) {
    this._showProgressDelayed();

    gChromeWin.Reader.getArticleForTab(tabId, url, function(article) {
      if (article)
        this._showContent(article);
      else
        this._showError(gStrings.GetStringFromName("aboutReader.loadError"));
    }.bind(this));
  },

  _requestFavicon: function Reader_requestFavicon() {
    gChromeWin.sendMessageToJava({
      type: "Reader:FaviconRequest",
      url: this._article.url
    });
  },

  _loadFavicon: function Reader_loadFavicon(url, faviconUrl) {
    if (this._article.url !== url)
      return;

    let doc = this._doc;

    let link = doc.createElement('link');
    link.rel = 'shortcut icon';
    link.href = faviconUrl;

    doc.getElementsByTagName('head')[0].appendChild(link);
  },

  _updateImageMargins: function Reader_updateImageMargins() {
    let windowWidth = this._win.innerWidth;
    let contentWidth = this._contentElement.offsetWidth;
    let maxWidthStyle = windowWidth + "px !important";

    let setImageMargins = function(img) {
      if (!img._originalWidth)
        img._originalWidth = img.offsetWidth;

      let imgWidth = img._originalWidth;

      // If the image is taking more than half of the screen, just make
      // it fill edge-to-edge.
      if (imgWidth < contentWidth && imgWidth > windowWidth * 0.55)
        imgWidth = windowWidth;

      let sideMargin = Math.max((contentWidth - windowWidth) / 2,
                                (contentWidth - imgWidth) / 2);

      let imageStyle = sideMargin + "px !important";
      let widthStyle = imgWidth + "px !important";

      let cssText = "max-width: " + maxWidthStyle + ";" +
                    "width: " + widthStyle + ";" +
                    "margin-left: " + imageStyle + ";" +
                    "margin-right: " + imageStyle + ";";

      img.style.cssText = cssText;
    }

    let imgs = this._doc.querySelectorAll(this._BLOCK_IMAGES_SELECTOR);
    for (let i = imgs.length; --i >= 0;) {
      let img = imgs[i];

      if (img.width > 0) {
        setImageMargins(img);
      } else {
        img.onload = function() {
          setImageMargins(img);
        }
      }
    }
  },

  _maybeSetTextDirection: function Read_maybeSetTextDirection(article){
    if(!article.dir)
      return;

    //Set "dir" attribute on content
    this._contentElement.setAttribute("dir", article.dir);
    this._headerElement.setAttribute("dir", article.dir);
  },

  _showError: function Reader_showError(error) {
    this._headerElement.style.display = "none";
    this._contentElement.style.display = "none";

    this._messageElement.innerHTML = error;
    this._messageElement.style.display = "block";

    this._doc.title = error;
  },

  // This function is the JS version of Java's StringUtils.stripCommonSubdomains.
  _stripHost: function Reader_stripHost(host) {
    if (!host)
      return host;

    let start = 0;

    if (host.startsWith("www."))
      start = 4;
    else if (host.startsWith("m."))
      start = 2;
    else if (host.startsWith("mobile."))
      start = 7;

    return host.substring(start);
  },

  _showContent: function Reader_showContent(article) {
    this._messageElement.style.display = "none";

    this._article = article;

    this._domainElement.href = article.url;
    let articleUri = Services.io.newURI(article.url, null, null);
    this._domainElement.innerHTML = this._stripHost(articleUri.host);

    this._creditsElement.innerHTML = article.byline;

    this._titleElement.textContent = article.title;
    this._doc.title = article.title;

    this._headerElement.style.display = "block";

    let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);
    let contentFragment = parserUtils.parseFragment(article.content, Ci.nsIParserUtils.SanitizerDropForms,
                                                    false, articleUri, this._contentElement);
    this._contentElement.innerHTML = "";
    this._contentElement.appendChild(contentFragment);
    this._updateImageMargins();
    this._maybeSetTextDirection(article);

    this._contentElement.style.display = "block";
    this._requestReadingListStatus();

    this._toolbarEnabled = true;
    this._setToolbarVisibility(true);

    this._requestFavicon();
  },

  _hideContent: function Reader_hideContent() {
    this._headerElement.style.display = "none";
    this._contentElement.style.display = "none";
  },

  _showProgressDelayed: function Reader_showProgressDelayed() {
    this._win.setTimeout(function() {
      // Article has already been loaded, no need to show
      // progress anymore.
      if (this._article)
        return;

      this._headerElement.style.display = "none";
      this._contentElement.style.display = "none";

      this._messageElement.innerHTML = gStrings.GetStringFromName("aboutReader.loading");
      this._messageElement.style.display = "block";
    }.bind(this), 300);
  },

  _decodeQueryString: function Reader_decodeQueryString(url) {
    let result = {};
    let query = url.split("?")[1];
    if (query) {
      let pairs = query.split("&");
      for (let i = 0; i < pairs.length; i++) {
        let [name, value] = pairs[i].split("=");
        result[name] = decodeURIComponent(value);
      }
    }

    return result;
  },

  _setupSegmentedButton: function Reader_setupSegmentedButton(id, options, initialValue, callback) {
    let doc = this._doc;
    let segmentedButton = doc.getElementById(id);

    for (let i = 0; i < options.length; i++) {
      let option = options[i];

      let item = doc.createElement("li");
      let link = doc.createElement("a");
      link.textContent = option.name;
      item.appendChild(link);

      if (option.linkClass !== undefined)
        link.classList.add(option.linkClass);

      if (option.description !== undefined) {
        let description = doc.createElement("div");
        description.textContent = option.description;
        item.appendChild(description);
      }

      link.style.MozUserSelect = 'none';
      segmentedButton.appendChild(item);

      link.addEventListener("click", function(aEvent) {
        if (!aEvent.isTrusted)
          return;

        aEvent.stopPropagation();

        let items = segmentedButton.children;
        for (let j = items.length - 1; j >= 0; j--) {
          items[j].classList.remove("selected");
        }

        item.classList.add("selected");
        callback(option.value);
      }.bind(this), true);

      if (option.value === initialValue)
        item.classList.add("selected");
    }
  },

  _setupButton: function Reader_setupButton(id, callback) {
    let button = this._doc.getElementById(id);

    button.addEventListener("click", function(aEvent) {
      if (!aEvent.isTrusted)
        return;

      aEvent.stopPropagation();
      callback();
    }, true);
  },

  _setupAllDropdowns: function Reader_setupAllDropdowns() {
    let doc = this._doc;
    let win = this._win;

    let dropdowns = doc.getElementsByClassName("dropdown");

    for (let i = dropdowns.length - 1; i >= 0; i--) {
      let dropdown = dropdowns[i];

      let dropdownToggle = dropdown.getElementsByClassName("dropdown-toggle")[0];
      let dropdownPopup = dropdown.getElementsByClassName("dropdown-popup")[0];

      if (!dropdownToggle || !dropdownPopup)
        continue;

      let dropdownArrow = doc.createElement("div");
      dropdownArrow.className = "dropdown-arrow";
      dropdownPopup.appendChild(dropdownArrow);

      let updatePopupPosition = function() {
        let popupWidth = dropdownPopup.offsetWidth + 30;
        let arrowWidth = dropdownArrow.offsetWidth;
        let toggleWidth = dropdownToggle.offsetWidth;
        let toggleLeft = dropdownToggle.offsetLeft;

        let popupShift = (toggleWidth - popupWidth) / 2;
        let popupLeft = Math.max(0, Math.min(win.innerWidth - popupWidth, toggleLeft + popupShift));
        dropdownPopup.style.left = popupLeft + "px";

        let arrowShift = (toggleWidth - arrowWidth) / 2;
        let arrowLeft = toggleLeft - popupLeft + arrowShift;
        dropdownArrow.style.left = arrowLeft + "px";
      };

      win.addEventListener("resize", function(aEvent) {
        if (!aEvent.isTrusted)
          return;

        // Wait for reflow before calculating the new position of the popup.
        setTimeout(updatePopupPosition, 0);
      }, true);

      dropdownToggle.addEventListener("click", function(aEvent) {
        if (!aEvent.isTrusted)
          return;

        aEvent.stopPropagation();

        if (!this._getToolbarVisibility())
          return;

        let dropdownClasses = dropdown.classList;

        if (dropdownClasses.contains("open")) {
          win.history.back();
        } else {
          updatePopupPosition();
          if (!this._closeAllDropdowns())
            this._pushDropdownState();

          dropdownClasses.add("open");
        }
      }.bind(this), true);
    }
  },

  _pushDropdownState: function Reader_pushDropdownState() {
    // FIXME: We're getting a NS_ERROR_UNEXPECTED error when we try
    // to do win.history.pushState() here (see bug 682296). This is
    // a workaround that allows us to push history state on the target
    // content document.

    let doc = this._doc;
    let body = doc.body;

    if (this._pushStateScript)
      body.removeChild(this._pushStateScript);

    this._pushStateScript = doc.createElement('script');
    this._pushStateScript.type = "text/javascript";
    this._pushStateScript.innerHTML = 'history.pushState({ dropdown: 1 }, document.title);';

    body.appendChild(this._pushStateScript);
  },

  _closeAllDropdowns : function Reader_closeAllDropdowns() {
    let dropdowns = this._doc.querySelectorAll(".dropdown.open");
    for (let i = dropdowns.length - 1; i >= 0; i--) {
      dropdowns[i].classList.remove("open");
    }

    return (dropdowns.length > 0)
  }
};
