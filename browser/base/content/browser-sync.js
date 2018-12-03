/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

ChromeUtils.import("resource://services-sync/UIState.jsm");

ChromeUtils.defineModuleGetter(this, "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
ChromeUtils.defineModuleGetter(this, "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm");
ChromeUtils.defineModuleGetter(this, "Weave",
  "resource://services-sync/main.js");

const MIN_STATUS_ANIMATION_DURATION = 1600;

var gSync = {
  _initialized: false,
  // The last sync start time. Used to calculate the leftover animation time
  // once syncing completes (bug 1239042).
  _syncStartTime: 0,
  _syncAnimationTimer: 0,

  _obs: [
    "weave:engine:sync:finish",
    "quit-application",
    UIState.ON_UPDATE,
  ],

  get fxaStrings() {
    delete this.fxaStrings;
    return this.fxaStrings = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
  },

  get syncStrings() {
    delete this.syncStrings;
    // XXXzpao these strings should probably be moved from /services to /browser... (bug 583381)
    //        but for now just make it work
    return this.syncStrings = Services.strings.createBundle(
      "chrome://weave/locale/sync.properties"
    );
  },

  get syncReady() {
    return Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject.ready;
  },

  // Returns true if sync is configured but hasn't loaded or is yet to determine
  // if any remote clients exist.
  get syncConfiguredAndLoading() {
    return UIState.get().status == UIState.STATUS_SIGNED_IN &&
           (!this.syncReady || Weave.Service.clientsEngine.isFirstSync);
  },

  get isSignedIn() {
    return UIState.get().status == UIState.STATUS_SIGNED_IN;
  },

  get sendTabTargets() {
    return Weave.Service.clientsEngine.fxaDevices
      .sort((a, b) => a.name.localeCompare(b.name))
      .filter(d => !d.isCurrentDevice && (fxAccounts.commands.sendTab.isDeviceCompatible(d) || d.clientRecord));
  },

  get offline() {
    return Weave.Service.scheduler.offline;
  },

  _generateNodeGetters() {
    for (let k of ["Status", "Avatar", "Label", "Container"]) {
      let prop = "appMenu" + k;
      let suffix = k.toLowerCase();
      delete this[prop];
      this.__defineGetter__(prop, function() {
        delete this[prop];
        return this[prop] = document.getElementById("appMenu-fxa-" + suffix);
      });
    }
  },

  _definePrefGetters() {
    XPCOMUtils.defineLazyPreferenceGetter(this, "UNSENDABLE_URL_REGEXP",
        "services.sync.engine.tabs.filteredUrls", null, null, rx => {
          try {
            return new RegExp(rx, "i");
          } catch (e) {
            Cu.reportError(`Failed to build url filter regexp for send tab: ${e}`);
            return null;
          }
        });
    XPCOMUtils.defineLazyPreferenceGetter(this, "PRODUCT_INFO_BASE_URL",
        "app.productInfo.baseURL");
    XPCOMUtils.defineLazyPreferenceGetter(this, "SYNC_ENABLED",
        "identity.fxaccounts.enabled");
  },

  _maybeUpdateUIState() {
    // Update the UI.
    if (UIState.isReady()) {
      const state = UIState.get();
      // If we are not configured, the UI is already in the right state when
      // we open the window. We can avoid a repaint.
      if (state.status != UIState.STATUS_NOT_CONFIGURED) {
        this.updateAllUI(state);
      }
    }
  },

  init() {
    if (this._initialized) {
      return;
    }

    this._definePrefGetters();

    if (!this.SYNC_ENABLED) {
      this.onSyncDisabled();
      return;
    }

    // Label for the sync buttons, also set on the icon for accessibility.
    let syncIcon = document.getElementById("appMenu-fxa-icon");
    if (!syncIcon) {
      // We are in a window without our elements - just abort now, without
      // setting this._initialized, so we don't attempt to remove observers.
      return;
    }
    let syncNow = document.getElementById("PanelUI-remotetabs-syncnow");
    let label = this.syncStrings.GetStringFromName("syncnow.label");
    syncIcon.setAttribute("label", label);
    syncNow.setAttribute("label", label);
    // We start with every menuitem hidden, so that we don't need to init
    // the sync UI on windows like pageInfo.xul (see bug 1384856).
    document.getElementById("sync-setup").hidden = false;

    for (let topic of this._obs) {
      Services.obs.addObserver(this, topic, true);
    }

    this._generateNodeGetters();

    this._maybeUpdateUIState();

    EnsureFxAccountsWebChannel();

    this._initialized = true;
  },

  uninit() {
    if (!this._initialized) {
      return;
    }

    for (let topic of this._obs) {
      Services.obs.removeObserver(this, topic);
    }

    this._initialized = false;
  },

  observe(subject, topic, data) {
    if (!this._initialized) {
      Cu.reportError("browser-sync observer called after unload: " + topic);
      return;
    }
    switch (topic) {
      case UIState.ON_UPDATE:
        const state = UIState.get();
        this.updateAllUI(state);
        break;
      case "quit-application":
        // Stop the animation timer on shutdown, since we can't update the UI
        // after this.
        clearTimeout(this._syncAnimationTimer);
        break;
      case "weave:engine:sync:finish":
        if (data != "clients") {
          return;
        }
        this.onClientsSynced();
        break;
    }
  },

  updateAllUI(state) {
    this.updatePanelPopup(state);
    this.updateState(state);
    this.updateSyncButtonsTooltip(state);
    this.updateSyncStatus(state);
  },

  updatePanelPopup(state) {
    let defaultLabel = this.appMenuStatus.getAttribute("defaultlabel");
    // The localization string is for the signed in text, but it's the default text as well
    let defaultTooltiptext = this.appMenuStatus.getAttribute("signedinTooltiptext");

    const status = state.status;
    // Reset the status bar to its original state.
    this.appMenuLabel.setAttribute("label", defaultLabel);
    this.appMenuStatus.setAttribute("tooltiptext", defaultTooltiptext);
    this.appMenuContainer.removeAttribute("fxastatus");
    this.appMenuAvatar.style.removeProperty("list-style-image");

    if (status == UIState.STATUS_NOT_CONFIGURED) {
      return;
    }

    // At this point we consider sync to be configured (but still can be in an error state).
    if (status == UIState.STATUS_LOGIN_FAILED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName("reconnectDescription", [state.email], 1);
      let errorLabel = this.appMenuStatus.getAttribute("errorlabel");
      this.appMenuContainer.setAttribute("fxastatus", "login-failed");
      this.appMenuLabel.setAttribute("label", errorLabel);
      this.appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      return;
    } else if (status == UIState.STATUS_NOT_VERIFIED) {
      let tooltipDescription = this.fxaStrings.formatStringFromName("verifyDescription", [state.email], 1);
      let unverifiedLabel = this.appMenuStatus.getAttribute("unverifiedlabel");
      this.appMenuContainer.setAttribute("fxastatus", "unverified");
      this.appMenuLabel.setAttribute("label", unverifiedLabel);
      this.appMenuStatus.setAttribute("tooltiptext", tooltipDescription);
      return;
    }

    // At this point we consider sync to be logged-in.
    this.appMenuContainer.setAttribute("fxastatus", "signedin");
    this.appMenuLabel.setAttribute("label", state.displayName || state.email);

    if (state.avatarURL) {
      let bgImage = "url(\"" + state.avatarURL + "\")";
      this.appMenuAvatar.style.listStyleImage = bgImage;

      let img = new Image();
      img.onerror = () => {
        // Clear the image if it has trouble loading. Since this callback is asynchronous
        // we check to make sure the image is still the same before we clear it.
        if (this.appMenuAvatar.style.listStyleImage === bgImage) {
          this.appMenuAvatar.style.removeProperty("list-style-image");
        }
      };
      img.src = state.avatarURL;
    }
  },

  updateState(state) {
    for (let [status, menuId, boxId] of [
      [UIState.STATUS_NOT_CONFIGURED, "sync-setup",
                                      "PanelUI-remotetabs-setupsync"],
      [UIState.STATUS_LOGIN_FAILED,   "sync-reauthitem",
                                      "PanelUI-remotetabs-reauthsync"],
      [UIState.STATUS_NOT_VERIFIED,   "sync-unverifieditem",
                                      "PanelUI-remotetabs-unverified"],
      [UIState.STATUS_SIGNED_IN,      "sync-syncnowitem",
                                      "PanelUI-remotetabs-main"],
    ]) {
      document.getElementById(menuId).hidden =
        document.getElementById(boxId).hidden = (status != state.status);
    }
  },

  updateSyncStatus(state) {
    let syncNow = document.getElementById("PanelUI-remotetabs-syncnow");
    const syncingUI = syncNow.getAttribute("syncstatus") == "active";
    if (state.syncing != syncingUI) { // Do we need to update the UI?
      state.syncing ? this.onActivityStart() : this.onActivityStop();
    }
  },

  onMenuPanelCommand() {
    switch (this.appMenuContainer.getAttribute("fxastatus")) {
    case "signedin":
      this.openPrefs("menupanel", "fxaSignedin");
      break;
    case "error":
      if (this.appMenuContainer.getAttribute("fxastatus") == "unverified") {
        this.openPrefs("menupanel", "fxaError");
      } else {
        this.openSignInAgainPage("menupanel");
      }
      break;
    default:
      this.openPrefs("menupanel", "fxa");
      break;
    }

    PanelUI.hide();
  },

  async openSignInAgainPage(entryPoint) {
    const url = await FxAccounts.config.promiseForceSigninURI(entryPoint);
    switchToTabHavingURI(url, true, {
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  async openDevicesManagementPage(entryPoint) {
    let url = await FxAccounts.config.promiseManageDevicesURI(entryPoint);
    switchToTabHavingURI(url, true, {
      replaceQueryString: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  async openConnectAnotherDevice(entryPoint) {
    const url = await FxAccounts.config.promiseConnectDeviceURI(entryPoint);
    openTrustedLinkIn(url, "tab");
  },

  openSendToDevicePromo() {
    let url = this.PRODUCT_INFO_BASE_URL;
    url += "send-tabs/?utm_source=" + Services.appinfo.name.toLowerCase();
    switchToTabHavingURI(url, true, { replaceQueryString: true });
  },

  async sendTabToDevice(url, targets, title) {
    const fxaCommandsDevices = [];
    const oldSendTabClients = [];
    for (const target of targets) {
      if (fxAccounts.commands.sendTab.isDeviceCompatible(target)) {
        fxaCommandsDevices.push(target);
      } else if (target.clientRecord) {
        oldSendTabClients.push(target.clientRecord);
      } else {
        console.error(`Target ${target.id} unsuitable for send tab.`);
      }
    }
    if (fxaCommandsDevices.length) {
      console.log(`Sending a tab to ${fxaCommandsDevices.map(d => d.name).join(", ")} using FxA commands.`);
      const report = await fxAccounts.commands.sendTab.send(fxaCommandsDevices, {url, title});
      for (let {device, error} of report.failed) {
        console.error(`Failed to send a tab with FxA commands for ${device.name}.
                       Falling back on the Sync back-end`, error);
        if (!device.clientRecord) {
          console.error(`Could not find associated Sync device for ${device.name}`);
          continue;
        }
        oldSendTabClients.push(device.clientRecord);
      }
    }
    for (let client of oldSendTabClients) {
      try {
        console.log(`Sending a tab to ${client.name} using Sync.`);
        await Weave.Service.clientsEngine.sendURIToClientForDisplay(url, client.id, title);
      } catch (e) {
        console.error("Could not send tab to device.", e);
      }
    }
  },

  populateSendTabToDevicesMenu(devicesPopup, url, title, multiselected, createDeviceNodeFn) {
    if (!createDeviceNodeFn) {
      createDeviceNodeFn = (targetId, name, targetType, lastModified) => {
        let eltName = name ? "menuitem" : "menuseparator";
        return document.createXULElement(eltName);
      };
    }

    // remove existing menu items
    for (let i = devicesPopup.children.length - 1; i >= 0; --i) {
      let child = devicesPopup.children[i];
      if (child.classList.contains("sync-menuitem")) {
        child.remove();
      }
    }

    if (gSync.syncConfiguredAndLoading) {
      // We can only be in this case in the page action menu.
      return;
    }

    const fragment = document.createDocumentFragment();

    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN && this.sendTabTargets.length > 0) {
      this._appendSendTabDeviceList(fragment, createDeviceNodeFn, url, title, multiselected);
    } else if (state.status == UIState.STATUS_SIGNED_IN) {
      this._appendSendTabSingleDevice(fragment, createDeviceNodeFn);
    } else if (state.status == UIState.STATUS_NOT_VERIFIED ||
               state.status == UIState.STATUS_LOGIN_FAILED) {
      this._appendSendTabVerify(fragment, createDeviceNodeFn);
    } else /* status is STATUS_NOT_CONFIGURED */ {
      this._appendSendTabUnconfigured(fragment, createDeviceNodeFn);
    }

    devicesPopup.appendChild(fragment);
  },

  // TODO: once our transition from the old-send tab world is complete,
  // this list should be built using the FxA device list instead of the client
  // collection.
  _appendSendTabDeviceList(fragment, createDeviceNodeFn, url, title, multiselected) {
    const targets = this.sendTabTargets;

    let tabsToSend = multiselected ?
      gBrowser.selectedTabs.map(t => {
        return {
          url: t.linkedBrowser.currentURI.spec,
          title: t.linkedBrowser.contentTitle,
        };
      }) : [{url, title}];

    const onSendAllCommand = (event) => {
      for (let t of tabsToSend) {
        this.sendTabToDevice(t.url, targets, t.title);
      }
    };
    const onTargetDeviceCommand = (event) => {
      const targetId = event.target.getAttribute("clientId");
      const target = targets.find(t => t.id == targetId);
      for (let t of tabsToSend) {
        this.sendTabToDevice(t.url, [target], t.title);
      }
    };

    function addTargetDevice(targetId, name, targetType, lastModified) {
      const targetDevice = createDeviceNodeFn(targetId, name, targetType, lastModified);
      targetDevice.addEventListener("command", targetId ? onTargetDeviceCommand :
                                                          onSendAllCommand, true);
      targetDevice.classList.add("sync-menuitem", "sendtab-target");
      targetDevice.setAttribute("clientId", targetId);
      targetDevice.setAttribute("clientType", targetType);
      targetDevice.setAttribute("label", name);
      fragment.appendChild(targetDevice);
    }

    for (let target of targets) {
      let type, lastModified;
      if (target.clientRecord) {
        type = Weave.Service.clientsEngine.getClientType(target.clientRecord.id);
        lastModified = new Date(target.clientRecord.serverLastModified * 1000);
      } else {
        type = target.type === "desktop" ? "desktop" : "phone"; // Normalizing the FxA types just in case.
        lastModified = null;
      }
      addTargetDevice(target.id, target.name, type, lastModified);
    }

    // "Send to All Devices" menu item
    if (targets.length > 1) {
      const separator = createDeviceNodeFn();
      separator.classList.add("sync-menuitem");
      fragment.appendChild(separator);
      const allDevicesLabel = this.fxaStrings.GetStringFromName("sendToAllDevices.menuitem");
      addTargetDevice("", allDevicesLabel, "");
    }
  },

  _appendSendTabSingleDevice(fragment, createDeviceNodeFn) {
    const noDevices = this.fxaStrings.GetStringFromName("sendTabToDevice.singledevice.status");
    const learnMore = this.fxaStrings.GetStringFromName("sendTabToDevice.singledevice");
    const connectDevice = this.fxaStrings.GetStringFromName("sendTabToDevice.connectdevice");
    const actions = [{label: connectDevice, command: () => this.openConnectAnotherDevice("sendtab")},
                     {label: learnMore,     command: () => this.openSendToDevicePromo()}];
    this._appendSendTabInfoItems(fragment, createDeviceNodeFn, noDevices, actions);
  },

  _appendSendTabVerify(fragment, createDeviceNodeFn) {
    const notVerified = this.fxaStrings.GetStringFromName("sendTabToDevice.verify.status");
    const verifyAccount = this.fxaStrings.GetStringFromName("sendTabToDevice.verify");
    const actions = [{label: verifyAccount, command: () => this.openPrefs("sendtab")}];
    this._appendSendTabInfoItems(fragment, createDeviceNodeFn, notVerified, actions);
  },

  _appendSendTabUnconfigured(fragment, createDeviceNodeFn) {
    const notConnected = this.fxaStrings.GetStringFromName("sendTabToDevice.unconfigured.status");
    const learnMore = this.fxaStrings.GetStringFromName("sendTabToDevice.unconfigured");
    const actions = [{label: learnMore, command: () => this.openSendToDevicePromo()}];
    this._appendSendTabInfoItems(fragment, createDeviceNodeFn, notConnected, actions);

    // Now add a 'sign in to sync' item above the 'learn more' item.
    const signInToSync = this.fxaStrings.GetStringFromName("sendTabToDevice.signintosync");
    let signInItem = createDeviceNodeFn(null, signInToSync, null);
    signInItem.classList.add("sync-menuitem");
    signInItem.setAttribute("label", signInToSync);
    // Show an icon if opened in the page action panel:
    if (signInItem.classList.contains("subviewbutton")) {
      signInItem.classList.add("subviewbutton-iconic", "signintosync");
    }
    signInItem.addEventListener("command", () => {
      this.openPrefs("sendtab");
    });
    fragment.insertBefore(signInItem, fragment.lastElementChild);
  },

  _appendSendTabInfoItems(fragment, createDeviceNodeFn, statusLabel, actions) {
    const status = createDeviceNodeFn(null, statusLabel, null);
    status.setAttribute("label", statusLabel);
    status.setAttribute("disabled", true);
    status.classList.add("sync-menuitem");
    fragment.appendChild(status);

    const separator = createDeviceNodeFn(null, null, null);
    separator.classList.add("sync-menuitem");
    fragment.appendChild(separator);

    for (let {label, command} of actions) {
      const actionItem = createDeviceNodeFn(null, label, null);
      actionItem.addEventListener("command", command, true);
      actionItem.classList.add("sync-menuitem");
      actionItem.setAttribute("label", label);
      fragment.appendChild(actionItem);
    }
  },

  isSendableURI(aURISpec) {
    if (!aURISpec) {
      return false;
    }
    // Disallow sending tabs with more than 65535 characters.
    if (aURISpec.length > 65535) {
      return false;
    }
    if (this.UNSENDABLE_URL_REGEXP) {
      return !this.UNSENDABLE_URL_REGEXP.test(aURISpec);
    }
    // The preference has been removed, or is an invalid regexp, so we treat it
    // as a valid URI. We've already logged an error when trying to construct
    // the regexp, and the more problematic case is the length, which we've
    // already addressed.
    return true;
  },

  // "Send Tab to Device" menu item
  updateTabContextMenu(aPopupMenu, aTargetTab) {
    // We may get here before initialisation. This situation
    // can lead to a empty label for 'Send To Device' Menu.
    this.init();

    if (!this.SYNC_ENABLED) {
      // These items are hidden in onSyncDisabled(). No need to do anything.
      return;
    }
    let hasASendableURI = false;
    for (let tab of aTargetTab.multiselected ? gBrowser.selectedTabs : [aTargetTab]) {
      if (this.isSendableURI(tab.linkedBrowser.currentURI.spec)) {
        hasASendableURI = true;
        break;
      }
    }
    const enabled = !this.syncConfiguredAndLoading && hasASendableURI;

    let sendTabsToDevice = document.getElementById("context_sendTabToDevice");
    sendTabsToDevice.disabled = !enabled;

    let tabCount = aTargetTab.multiselected ? gBrowser.multiSelectedTabsCount : 1;
    sendTabsToDevice.label = PluralForm.get(tabCount,
                                           gNavigatorBundle.getString("sendTabsToDevice.label"))
                                      .replace("#1", tabCount.toLocaleString());
    sendTabsToDevice.accessKey = gNavigatorBundle.getString("sendTabsToDevice.accesskey");
  },

  // "Send Page to Device" and "Send Link to Device" menu items
  updateContentContextMenu(contextMenu) {
    if (!this.SYNC_ENABLED) {
      // These items are hidden by default. No need to do anything.
      return;
    }
    // showSendLink and showSendPage are mutually exclusive
    const showSendLink = contextMenu.onSaveableLink || contextMenu.onPlainTextLink;
    const showSendPage = !showSendLink
                         && !(contextMenu.isContentSelected ||
                              contextMenu.onImage || contextMenu.onCanvas ||
                              contextMenu.onVideo || contextMenu.onAudio ||
                              contextMenu.onLink || contextMenu.onTextInput);

    // Avoids double separator on images with links.
    const hideSeparator = contextMenu.isContentSelected &&
                          contextMenu.onLink && contextMenu.onImage;
    ["context-sendpagetodevice", ...(hideSeparator ? [] : ["context-sep-sendpagetodevice"])]
    .forEach(id => contextMenu.showItem(id, showSendPage));
    ["context-sendlinktodevice", ...(hideSeparator ? [] : ["context-sep-sendlinktodevice"])]
    .forEach(id => contextMenu.showItem(id, showSendLink));

    if (!showSendLink && !showSendPage) {
      return;
    }

    const targetURI = showSendLink ? contextMenu.linkURL :
                                     contextMenu.browser.currentURI.spec;
    const enabled = !this.syncConfiguredAndLoading && this.isSendableURI(targetURI);
    contextMenu.setItemAttr(showSendPage ? "context-sendpagetodevice" :
                                           "context-sendlinktodevice",
                                           "disabled", !enabled || null);
  },

  // Functions called by observers
  onActivityStart() {
    clearTimeout(this._syncAnimationTimer);
    this._syncStartTime = Date.now();

    let label = this.syncStrings.GetStringFromName("syncingtabs.label");
    let syncIcon = document.getElementById("appMenu-fxa-icon");
    let syncNow = document.getElementById("PanelUI-remotetabs-syncnow");
    syncIcon.setAttribute("syncstatus", "active");
    syncIcon.setAttribute("label", label);
    syncIcon.setAttribute("disabled", "true");
    syncNow.setAttribute("syncstatus", "active");
    syncNow.setAttribute("label", label);
    syncNow.setAttribute("disabled", "true");
  },

  _onActivityStop() {
    if (!gBrowser)
      return;
    let label = this.syncStrings.GetStringFromName("syncnow.label");
    let syncIcon = document.getElementById("appMenu-fxa-icon");
    let syncNow = document.getElementById("PanelUI-remotetabs-syncnow");
    syncIcon.removeAttribute("syncstatus");
    syncIcon.removeAttribute("disabled");
    syncIcon.setAttribute("label", label);
    syncNow.removeAttribute("syncstatus");
    syncNow.removeAttribute("disabled");
    syncNow.setAttribute("label", label);
    Services.obs.notifyObservers(null, "test:browser-sync:activity-stop");
  },

  onActivityStop() {
    let now = Date.now();
    let syncDuration = now - this._syncStartTime;

    if (syncDuration < MIN_STATUS_ANIMATION_DURATION) {
      let animationTime = MIN_STATUS_ANIMATION_DURATION - syncDuration;
      clearTimeout(this._syncAnimationTimer);
      this._syncAnimationTimer = setTimeout(() => this._onActivityStop(), animationTime);
    } else {
      this._onActivityStop();
    }
  },

  // doSync forces a sync - it *does not* return a promise as it is called
  // via the various UI components.
  doSync() {
    if (!UIState.isReady()) {
      return;
    }
    const state = UIState.get();
    if (state.status == UIState.STATUS_SIGNED_IN) {
      this.updateSyncStatus({ syncing: true });
      Services.tm.dispatchToMainThread(() => {
        // We are pretty confident that push helps us pick up all FxA commands,
        // but some users might have issues with push, so let's unblock them
        // by fetching the missed FxA commands on manual sync.
        fxAccounts.commands.fetchMissedRemoteCommands().catch(e => {
          console.error("Fetching missed remote commands failed.", e);
        });
        Weave.Service.sync();
      });
    }
  },

  openPrefs(entryPoint = "syncbutton", origin = undefined) {
    window.openPreferences("paneSync", { origin, urlParams: { entrypoint: entryPoint } });
  },

  openSyncedTabsPanel() {
    let placement = CustomizableUI.getPlacementOfWidget("sync-button");
    let area = placement && placement.area;
    let anchor = document.getElementById("sync-button") ||
                 document.getElementById("PanelUI-menu-button");
    if (area == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
      // The button is in the overflow panel, so we need to show the panel,
      // then show our subview.
      let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
      navbar.overflowable.show().then(() => {
        PanelUI.showSubView("PanelUI-remotetabs", anchor);
      }, Cu.reportError);
    } else {
      // It is placed somewhere else - just try and show it.
      PanelUI.showSubView("PanelUI-remotetabs", anchor);
    }
  },

  refreshSyncButtonsTooltip() {
    const state = UIState.get();
    this.updateSyncButtonsTooltip(state);
  },

  /* Update the tooltip for the sync icon in the main menu and in Synced Tabs.
     If Sync is configured, the tooltip is when the last sync occurred,
     otherwise the tooltip reflects the fact that Sync needs to be
     (re-)configured.
  */
  updateSyncButtonsTooltip(state) {
    const status = state.status;

    // This is a little messy as the Sync buttons are 1/2 Sync related and
    // 1/2 FxA related - so for some strings we use Sync strings, but for
    // others we reach into gSync for strings.
    let tooltiptext;
    if (status == UIState.STATUS_NOT_VERIFIED) {
      // "needs verification"
      tooltiptext = this.fxaStrings.formatStringFromName("verifyDescription", [state.email], 1);
    } else if (status == UIState.STATUS_NOT_CONFIGURED) {
      // "needs setup".
      tooltiptext = this.syncStrings.GetStringFromName("signInToSync.description");
    } else if (status == UIState.STATUS_LOGIN_FAILED) {
      // "need to reconnect/re-enter your password"
      tooltiptext = this.fxaStrings.formatStringFromName("reconnectDescription", [state.email], 1);
    } else {
      // Sync appears configured - format the "last synced at" time.
      tooltiptext = this.formatLastSyncDate(state.lastSync);
    }

    let syncIcon = document.getElementById("appMenu-fxa-icon");
    if (syncIcon) {
      let syncNow = document.getElementById("PanelUI-remotetabs-syncnow");
      if (tooltiptext) {
        syncIcon.setAttribute("tooltiptext", tooltiptext);
        syncNow.setAttribute("tooltiptext", tooltiptext);
      } else {
        syncIcon.removeAttribute("tooltiptext");
        syncNow.removeAttribute("tooltiptext");
      }
    }
  },

  get relativeTimeFormat() {
    delete this.relativeTimeFormat;
    return this.relativeTimeFormat = new Services.intl.RelativeTimeFormat(undefined, {style: "long"});
  },

  formatLastSyncDate(date) {
    if (!date) { // Date can be null before the first sync!
      return null;
    }
    const relativeDateStr = this.relativeTimeFormat.formatBestUnit(date);
    return this.syncStrings.formatStringFromName("lastSync2.label", [relativeDateStr], 1);
  },

  onClientsSynced() {
    let element = document.getElementById("PanelUI-remotetabs-main");
    if (element) {
      if (Weave.Service.clientsEngine.stats.numClients > 1) {
        element.setAttribute("devices-status", "multi");
      } else {
        element.setAttribute("devices-status", "single");
      }
    }
  },

  onSyncDisabled() {
    const toHide = [...document.querySelectorAll(".sync-ui-item")];
    for (const item of toHide) {
      item.hidden = true;
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};
