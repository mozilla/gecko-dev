/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */
/* import-globals-from browser-siteProtections.js */

ChromeUtils.defineESModuleGetters(this, {
  ContentBlockingAllowList:
    "resource://gre/modules/ContentBlockingAllowList.sys.mjs",
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
  PanelMultiView: "resource:///modules/PanelMultiView.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  SiteDataManager: "resource:///modules/SiteDataManager.sys.mjs",
});

const ETP_ENABLED_ASSETS = {
  label: "trustpanel-etp-label-enabled",
  description: "trustpanel-etp-description-enabled",
  header: "trustpanel-header-enabled",
  innerDescription: "trustpanel-description-enabled",
};

const ETP_DISABLED_ASSETS = {
  label: "trustpanel-etp-label-disabled",
  description: "trustpanel-etp-description-disabled",
  header: "trustpanel-header-disabled",
  innerDescription: "trustpanel-description-disabled",
};

class TrustPanel {
  #popup = null;

  #state = null;
  #uri = null;

  #blockers = {
    SocialTracking,
    ThirdPartyCookies,
    TrackingProtection,
    Fingerprinting,
    Cryptomining,
  };

  constructor() {
    for (let blocker of Object.values(this.#blockers)) {
      if (blocker.init) {
        blocker.init();
      }
    }
  }

  handleProtectionsButtonEvent(event) {
    event.stopPropagation();
    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return; // Left click, space or enter only
    }

    this.showPopup({ event, openingReason: "shieldButtonClicked" });
  }

  onContentBlockingEvent(event, _webProgress, _isSimulated, _previousState) {
    // First update all our internal state based on the allowlist and the
    // different blockers:
    this.anyDetected = false;
    this.anyBlocking = false;
    this._lastEvent = event;

    // Check whether the user has added an exception for this site.
    this.hasException =
      ContentBlockingAllowList.canHandle(window.gBrowser.selectedBrowser) &&
      ContentBlockingAllowList.includes(window.gBrowser.selectedBrowser);

    // Update blocker state and find if they detected or blocked anything.
    for (let blocker of Object.values(this.#blockers)) {
      // Store data on whether the blocker is activated for reporting it
      // using the "report breakage" dialog. Under normal circumstances this
      // dialog should only be able to open in the currently selected tab
      // and onSecurityChange runs on tab switch, so we can avoid associating
      // the data with the document directly.
      blocker.activated = blocker.isBlocking(event);
      this.anyDetected = this.anyDetected || blocker.isDetected(event);
      this.anyBlocking = this.anyBlocking || blocker.activated;
    }
  }

  #initializePopup() {
    if (!this.#popup) {
      let wrapper = document.getElementById("template-trustpanel-popup");
      this.#popup = wrapper.content.firstElementChild;
      wrapper.replaceWith(wrapper.content);

      document
        .getElementById("trustpanel-toggle")
        .addEventListener("toggle", () => this.#toggleTrackingProtection());
      document
        .getElementById("trustpanel-popup-connection")
        .addEventListener("click", event =>
          this.#openSiteInformationSubview(event)
        );
      document
        .getElementById("trustpanel-blocker-see-all")
        .addEventListener("click", event => this.#openBlockerSubview(event));
      document
        .getElementById("trustpanel-privacy-link")
        .addEventListener("click", () =>
          window.openTrustedLinkIn("about:preferences#privacy", "tab")
        );
      document
        .getElementById("trustpanel-clear-cookies-button")
        .addEventListener("click", event =>
          this.#showClearCookiesSubview(event)
        );
      document
        .getElementById("trustpanel-siteinformation-morelink")
        .addEventListener("click", () => this.#showSecurityPopup());
      document
        .getElementById("trustpanel-clear-cookie-cancel")
        .addEventListener("click", () => this.#hidePopup());
      document
        .getElementById("trustpanel-clear-cookie-clear")
        .addEventListener("click", () => this.#clearSiteData());
      document
        .getElementById("trustpanel-toggle")
        .addEventListener("click", () => this.#toggleTrackingProtection());
    }
  }

  showPopup({ event }) {
    this.#initializePopup();
    this.#updatePopup();

    let opts = { position: "bottomleft topleft" };
    PanelMultiView.openPopup(this.#popup, event.target, opts);
  }

  async #hidePopup() {
    let hidden = new Promise(c => {
      this.#popup.addEventListener("popuphidden", c, { once: true });
    });
    PanelMultiView.hidePopup(this.#popup);
    await hidden;
  }

  updateIdentity(state, uri) {
    this.#state = state;
    this.#uri = uri;
    this.#updateUrlbarIcon();
  }

  #updateUrlbarIcon() {
    let icon = document.getElementById("trust-icon-container");
    let secureConnection = this.#isSecurePage();
    icon.className = "";

    if (!this.#trackingProtectionEnabled) {
      icon.classList.add("inactive");
    } else if (secureConnection && this.#trackingProtectionEnabled) {
      icon.classList.add("secure");
    } else if (!secureConnection || !this.#trackingProtectionEnabled) {
      icon.classList.add("insecure");
    } else {
      icon.classList.add("warning");
    }

    let chickletShown = this.#uri.schemeIs("moz-extension");
    if (this.#uri.schemeIs("about")) {
      let module = E10SUtils.getAboutModule(this.#uri);
      if (module) {
        let flags = module.getURIFlags(this.#uri);
        chickletShown = !!(flags & Ci.nsIAboutModule.IS_SECURE_CHROME_UI);
      }
    }
    icon.classList.toggle("chickletShown", chickletShown);
  }

  async #updatePopup() {
    let secureConnection = this.#isSecurePage();

    let connection = "not-secure";
    if (secureConnection || this.#isInternalSecurePage(this.#uri)) {
      connection = "secure";
    }

    this.#popup.setAttribute("connection", connection);
    this.#popup.setAttribute(
      "tracking-protection",
      this.#trackingProtectionStatus()
    );

    let assets = this.#trackingProtectionEnabled
      ? ETP_ENABLED_ASSETS
      : ETP_DISABLED_ASSETS;
    let host = window.gIdentityHandler.getHostForDisplay();
    this.host = host;

    let favicon = await PlacesUtils.favicons.getFaviconForPage(this.#uri);
    document.getElementById("trustpanel-popup-icon").src =
      favicon?.uri.spec ?? "";

    document
      .getElementById("trustpanel-toggle")
      .toggleAttribute("pressed", this.#trackingProtectionEnabled);

    document.getElementById("trustpanel-popup-host").textContent = host;

    document.l10n.setAttributes(
      document.getElementById("trustpanel-etp-label"),
      assets.label
    );
    document.l10n.setAttributes(
      document.getElementById("trustpanel-etp-description"),
      assets.description
    );
    document.l10n.setAttributes(
      document.getElementById("trustpanel-header"),
      assets.header
    );
    document.l10n.setAttributes(
      document.getElementById("trustpanel-description"),
      assets.innerDescription
    );
    document.l10n.setAttributes(
      document.getElementById("trustpanel-connection-label"),
      secureConnection
        ? "trustpanel-connection-label-secure"
        : "trustpanel-connection-label-insecure"
    );

    let canHandle = ContentBlockingAllowList.canHandle(
      window.gBrowser.selectedBrowser
    );
    document
      .getElementById("trustpanel-toggle")
      .toggleAttribute("disabled", !canHandle);
    document
      .getElementById("trustpanel-toggle-section")
      .toggleAttribute("disabled", !canHandle);

    if (!this.anyDetected) {
      document.getElementById("trustpanel-blocker-section").hidden = true;
    } else {
      let count = 0;
      let blocked = [];
      let detected = [];

      for (let blocker of Object.values(this.#blockers)) {
        if (blocker.isBlocking(this._lastEvent)) {
          blocked.push(blocker);
          count += await blocker.getBlockerCount();
        } else if (blocker.isDetected(this._lastEvent)) {
          detected.push(blocker);
        }
      }
      document.l10n.setArgs(
        document.getElementById("trustpanel-blocker-section-header"),
        { count }
      );
      this.#addButtons("trustpanel-blocked", blocked, true);
      this.#addButtons("trustpanel-detected", detected, false);

      document
        .getElementById("trustpanel-blocker-section")
        .removeAttribute("hidden");
    }
  }

  async #showSecurityPopup() {
    await this.#hidePopup();
    window.BrowserCommands.pageInfo(null, "securityTab");
  }

  #trackingProtectionStatus() {
    if (!this.#isSecurePage()) {
      return "warning";
    }
    return this.#trackingProtectionEnabled ? "enabled" : "disabled";
  }

  #openSiteInformationSubview(event) {
    let secureConnection =
      this.#state & Ci.nsIWebProgressListener.STATE_IS_SECURE;

    document.l10n.setAttributes(
      document.getElementById("trustpanel-siteInformationView"),
      "trustpanel-site-information-header",
      { host: this.host }
    );

    document.l10n.setAttributes(
      document.getElementById("trustpanel-siteinfo-label"),
      secureConnection
        ? "trustpanel-connection-secure"
        : "trustpanel-connection-not-secure"
    );

    document
      .getElementById("trustpanel-popup-multiView")
      .showSubView("trustpanel-siteInformationView", event.target);
  }

  async #openBlockerSubview(event) {
    document.l10n.setAttributes(
      document.getElementById("trustpanel-blockerView"),
      "trustpanel-blocker-header",
      { host: this.host }
    );
    document
      .getElementById("trustpanel-popup-multiView")
      .showSubView("trustpanel-blockerView", event.target);
  }

  async #openBlockerDetailsSubview(event, blocker, blocking) {
    let count = await blocker.getBlockerCount();
    let blockingKey = blocking ? "blocking" : "not-blocking";
    document.l10n.setAttributes(
      document.getElementById("trustpanel-blockerDetailsView"),
      `protections-${blockingKey}-${blocker.l10nKeys.title}`
    );
    document.l10n.setAttributes(
      document.getElementById("trustpanel-blocker-details-header"),
      `trustpanel-${blocker.l10nKeys.general}-${blockingKey}-tab-header`,
      { count }
    );
    document.l10n.setAttributes(
      document.getElementById("trustpanel-blocker-details-content"),
      `protections-panel-${blocker.l10nKeys.content}`
    );
    let header = blocker.l10nKeys.general;
    // These sections use the same string so reuse the same l10n key.
    if (["cookies", "tracking-cookies", "social-tracking"].includes(header)) {
      header = "tracking";
    }
    document.l10n.setAttributes(
      document.getElementById("trustpanel-blocker-details-list-header"),
      `trustpanel-${blocker.l10nKeys.general}-tab-list-header`
    );

    let { items } = await blocker._generateSubViewListItems();
    document.getElementById("trustpanel-blocker-items").replaceChildren(items);
    document
      .getElementById("trustpanel-popup-multiView")
      .showSubView("trustpanel-blockerDetailsView", event.target);
  }

  async #showClearCookiesSubview(event) {
    document.l10n.setAttributes(
      document.getElementById("trustpanel-clearcookiesView"),
      "trustpanel-clear-cookies-header",
      { host: window.gIdentityHandler.getHostForDisplay() }
    );
    document
      .getElementById("trustpanel-popup-multiView")
      .showSubView("trustpanel-clearcookiesView", event.target);
  }

  async #addButtons(section, blockers, blocking) {
    let sectionElement = document.getElementById(section);

    if (!blockers.length) {
      sectionElement.hidden = true;
      return;
    }

    let children = blockers.map(async blocker => {
      let button = document.createElement("moz-button");
      button.setAttribute("iconsrc", blocker.iconSrc);
      button.setAttribute("type", "ghost icon");
      document.l10n.setAttributes(
        button,
        `trustpanel-list-label-${blocker.l10nKeys.general}`,
        { count: await blocker.getBlockerCount() }
      );
      button.addEventListener("click", event =>
        this.#openBlockerDetailsSubview(event, blocker, blocking)
      );
      return button;
    });

    sectionElement.hidden = false;
    sectionElement
      .querySelector(".trustpanel-blocker-buttons")
      .replaceChildren(...(await Promise.all(children)));
  }

  get #trackingProtectionEnabled() {
    return !(
      ContentBlockingAllowList.canHandle(window.gBrowser.selectedBrowser) &&
      ContentBlockingAllowList.includes(window.gBrowser.selectedBrowser)
    );
  }

  #isSecurePage() {
    return (
      this.#state & Ci.nsIWebProgressListener.STATE_IS_SECURE ||
      this.#isInternalSecurePage(this.#uri)
    );
  }

  #isInternalSecurePage(uri) {
    if (uri.schemeIs("about")) {
      let module = E10SUtils.getAboutModule(uri);
      if (module) {
        let flags = module.getURIFlags(uri);
        if (flags & Ci.nsIAboutModule.IS_SECURE_CHROME_UI) {
          return true;
        }
      }
    }
    return false;
  }

  #clearSiteData() {
    let baseDomain = SiteDataManager.getBaseDomainFromHost(this.#uri.host);
    SiteDataManager.remove(baseDomain);
    this.#hidePopup();
  }

  #toggleTrackingProtection(enable) {
    if (enable) {
      ContentBlockingAllowList.remove(window.gBrowser.selectedBrowser);
    } else {
      ContentBlockingAllowList.add(window.gBrowser.selectedBrowser);
    }

    PanelMultiView.hidePopup(this.#popup);
    window.BrowserCommands.reload();
  }
}

var gTrustPanelHandler = new TrustPanel();
