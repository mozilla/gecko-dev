/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gPluginHandler = {
  PREF_SESSION_PERSIST_MINUTES: "plugin.sessionPermissionNow.intervalInMinutes",
  PREF_PERSISTENT_DAYS: "plugin.persistentPermissionAlways.intervalInDays",

  MESSAGES: [
    "PluginContent:ShowClickToPlayNotification",
    "PluginContent:RemoveNotification",
    "PluginContent:InstallSinglePlugin",
    "PluginContent:ShowPluginCrashedNotification",
    "PluginContent:SubmitReport",
    "PluginContent:LinkClickCallback",
  ],

  init() {
    const mm = window.messageManager;
    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }
    window.addEventListener("unload", this);
  },

  uninit() {
    const mm = window.messageManager;
    for (let msg of this.MESSAGES) {
      mm.removeMessageListener(msg, this);
    }
    window.removeEventListener("unload", this);
  },

  handleEvent(event) {
    if (event.type == "unload") {
      this.uninit();
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "PluginContent:ShowClickToPlayNotification":
        this.showClickToPlayNotification(msg.target, msg.data.plugins, msg.data.showNow,
                                         msg.principal, msg.data.location);
        break;
      case "PluginContent:RemoveNotification":
        this.removeNotification(msg.target, msg.data.name);
        break;
      case "PluginContent:InstallSinglePlugin":
        this.installSinglePlugin(msg.data.pluginInfo);
        break;
      case "PluginContent:ShowPluginCrashedNotification":
        this.showPluginCrashedNotification(msg.target, msg.data.messageString,
                                           msg.data.pluginID);
        break;
      case "PluginContent:SubmitReport":
        if (AppConstants.MOZ_CRASHREPORTER) {
          this.submitReport(msg.data.runID, msg.data.keyVals, msg.data.submitURLOptIn);
        }
        break;
      case "PluginContent:LinkClickCallback":
        switch (msg.data.name) {
          case "managePlugins":
          case "openHelpPage":
          case "openPluginUpdatePage":
            this[msg.data.name](msg.data.pluginTag);
            break;
        }
        break;
      default:
        Cu.reportError("gPluginHandler did not expect to handle message " + msg.name);
        break;
    }
  },

  // Callback for user clicking on a disabled plugin
  managePlugins() {
    BrowserOpenAddonsMgr("addons://list/plugin");
  },

  // Callback for user clicking on the link in a click-to-play plugin
  // (where the plugin has an update)
  async openPluginUpdatePage(pluginTag) {
    let { Blocklist } = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", {});
    let url = await Blocklist.getPluginBlockURL(pluginTag);
    openTrustedLinkIn(url, "tab");
  },

  submitReport: function submitReport(runID, keyVals, submitURLOptIn) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }
    Services.prefs.setBoolPref("dom.ipc.plugins.reportCrashURL", submitURLOptIn);
    PluginCrashReporter.submitCrashReport(runID, keyVals);
  },

  // Callback for user clicking a "reload page" link
  reloadPage(browser) {
    browser.reload();
  },

  // Callback for user clicking the help icon
  openHelpPage() {
    openHelpLink("plugin-crashed", false);
  },

  _clickToPlayNotificationEventCallback: function PH_ctpEventCallback(event) {
    if (event == "showing") {
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_SHOWN")
        .add(!this.options.primaryPlugin);
      // Histograms always start at 0, even though our data starts at 1
      let histogramCount = this.options.pluginData.size - 1;
      if (histogramCount > 4) {
        histogramCount = 4;
      }
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_PLUGIN_COUNT")
        .add(histogramCount);
    } else if (event == "dismissed") {
      // Once the popup is dismissed, clicking the icon should show the full
      // list again
      this.options.primaryPlugin = null;
    }
  },

  /**
   * Called from the plugin doorhanger to set the new permissions for a plugin
   * and activate plugins if necessary.
   * aNewState should be either "allownow" "allowalways" or "block"
   */
  _updatePluginPermission(aBrowser, aPluginInfo, aNewState) {
    let permission;
    let expireType;
    let expireTime;
    let histogram =
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_USER_ACTION_2");

    let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);

    // Update the permission manager.
    // Also update the current state of pluginInfo.fallbackType so that
    // subsequent opening of the notification shows the current state.
    switch (aNewState) {
      case "allownow":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_SESSION;
        expireTime = Date.now() + Services.prefs.getIntPref(this.PREF_SESSION_PERSIST_MINUTES) * 60 * 1000;
        histogram.add(0);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        notification.options.extraAttr = "active";
        break;

      case "allowalways":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_TIME;
        expireTime = Date.now() +
          Services.prefs.getIntPref(this.PREF_PERSISTENT_DAYS) * 24 * 60 * 60 * 1000;
        histogram.add(1);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        notification.options.extraAttr = "active";
        break;

      case "block":
        permission = Ci.nsIPermissionManager.PROMPT_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_NEVER;
        expireTime = 0;
        histogram.add(2);
        switch (aPluginInfo.blocklistState) {
          case Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE;
            break;
          case Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE;
            break;
          default:
            // PLUGIN_CLICK_TO_PLAY_QUIET will only last until they reload the page, at
            // which point it will be PLUGIN_CLICK_TO_PLAY (the overlays will appear)
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET;
        }
        notification.options.extraAttr = "inactive";
        break;

      case "blockalways":
        permission = Ci.nsIObjectLoadingContent.PLUGIN_PERMISSION_PROMPT_ACTION_QUIET;
        expireType = Ci.nsIPermissionManager.EXPIRE_NEVER;
        expireTime = 0;
        histogram.add(3);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET;
        notification.options.extraAttr = "inactive";
        break;

      // In case a plugin has already been allowed/disallowed in another tab, the
      // buttons matching the existing block state shouldn't change any permissions
      // but should run the plugin-enablement code below.
      case "continue":
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        notification.options.extraAttr = "active";
        break;

      case "continueblocking":
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET;
        notification.options.extraAttr = "inactive";
        break;

      default:
        Cu.reportError(Error("Unexpected plugin state: " + aNewState));
        return;
    }

    if (aNewState != "continue" && aNewState != "continueblocking") {
      let principal = notification.options.principal;
      Services.perms.addFromPrincipal(principal, aPluginInfo.permissionString,
                                      permission, expireType, expireTime);
      aPluginInfo.pluginPermissionType = expireType;
    }

    aBrowser.messageManager.sendAsyncMessage("BrowserPlugins:ActivatePlugins", {
      pluginInfo: aPluginInfo,
      newState: aNewState,
    });
  },

  showClickToPlayNotification(browser, plugins, showNow,
                                        principal, location) {
    // It is possible that we've received a message from the frame script to show
    // a click to play notification for a principal that no longer matches the one
    // that the browser's content now has assigned (ie, the browser has browsed away
    // after the message was sent, but before the message was received). In that case,
    // we should just ignore the message.
    if (!principal.equals(browser.contentPrincipal)) {
      return;
    }

    // Data URIs, when linked to from some page, inherit the principal of that
    // page. That means that we also need to compare the actual locations to
    // ensure we aren't getting a message from a Data URI that we're no longer
    // looking at.
    let receivedURI = Services.io.newURI(location);
    if (!browser.documentURI.equalsExceptRef(receivedURI)) {
      return;
    }

    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);

    // If this is a new notification, create a pluginData map, otherwise append
    let pluginData;
    if (notification) {
      pluginData = notification.options.pluginData;
    } else {
      pluginData = new Map();
    }

    for (let pluginInfo of plugins) {
      if (pluginData.has(pluginInfo.permissionString)) {
        continue;
      }
      pluginData.set(pluginInfo.permissionString, pluginInfo);
    }

    let primaryPluginPermission = null;
    if (showNow) {
      primaryPluginPermission = plugins[0].permissionString;
    }

    if (notification) {
      // Don't modify the notification UI while it's on the screen, that would be
      // jumpy and might allow clickjacking.
      if (showNow) {
        notification.options.primaryPlugin = primaryPluginPermission;
        notification.reshow();
      }
      return;
    }

    if (plugins.length == 1) {
      let pluginInfo = plugins[0];
      let isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);

      let active = pluginInfo.fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;

      let options = {
        dismissed: !showNow,
        hideClose: !Services.prefs.getBoolPref("privacy.permissionPrompts.showCloseButton"),
        persistent: showNow,
        eventCallback: this._clickToPlayNotificationEventCallback,
        primaryPlugin: primaryPluginPermission,
        popupIconClass: "plugin-icon",
        extraAttr: active ? "active" : "inactive",
        pluginData,
        principal,
      };

      let description;
      if (pluginInfo.fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE) {
        description = gNavigatorBundle.getString("flashActivate.outdated.message");
      } else {
        description = gNavigatorBundle.getString("flashActivate.message");
      }

      let badge = document.getElementById("plugin-icon-badge");
      badge.setAttribute("animate", "true");
      badge.addEventListener("animationend", function animListener(event) {
        if (event.animationName == "blink-badge" &&
            badge.hasAttribute("animate")) {
          badge.removeAttribute("animate");
          badge.removeEventListener("animationend", animListener);
        }
      });

      let weakBrowser = Cu.getWeakReference(browser);
      let mainAction = {
        callback: ({checkboxChecked}) => {
          let browserRef = weakBrowser.get();
          if (browserRef) {
            if (checkboxChecked) {
              this._updatePluginPermission(browserRef, pluginInfo, "allowalways");
            } else if (pluginInfo.fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE) {
              this._updatePluginPermission(browserRef, pluginInfo, "continue");
            } else {
              this._updatePluginPermission(browserRef, pluginInfo, "allownow");
            }
          }
        },
        label: gNavigatorBundle.getString("flashActivate.allow"),
        accessKey: gNavigatorBundle.getString("flashActivate.allow.accesskey"),
        dismiss: true,
      };

      let secondaryActions = null;
      if (!isWindowPrivate) {
        options.checkbox = {
          label: gNavigatorBundle.getString("flashActivate.remember"),
        };
        secondaryActions = [{
          callback: ({checkboxChecked}) => {
            let browserRef = weakBrowser.get();
            if (browserRef) {
              if (checkboxChecked) {
                this._updatePluginPermission(browserRef, pluginInfo, "blockalways");
              } else if (pluginInfo.fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE) {
                this._updatePluginPermission(browserRef, pluginInfo, "block");
              } else {
                this._updatePluginPermission(browserRef, pluginInfo, "continueblocking");
              }
            }
          },
          label: gNavigatorBundle.getString("flashActivate.noAllow"),
          accessKey: gNavigatorBundle.getString("flashActivate.noAllow.accesskey"),
          dismiss: true,
        }];
      }

      PopupNotifications.show(browser, "click-to-play-plugins",
                                             description, "plugins-notification-icon",
                                             mainAction, secondaryActions, options);

      // Check if the plugin is insecure and update the notification icon accordingly.
      let haveInsecure = false;
      switch (pluginInfo.fallbackType) {
        // haveInsecure will trigger the red flashing icon and the infobar
        // styling below
        case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
        case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
          haveInsecure = true;
      }

      document.getElementById("plugins-notification-icon").classList.
        toggle("plugin-blocked", haveInsecure);
    } else {
      this.removeNotification(browser, "click-to-play-plugins");
    }
  },

  removeNotification(browser, name) {
    let notification = PopupNotifications.getNotification(name, browser);
    if (notification)
      PopupNotifications.remove(notification);
  },

  contextMenuCommand(browser, plugin, command) {
    browser.messageManager.sendAsyncMessage("BrowserPlugins:ContextMenuCommand",
      { command }, { plugin });
  },

  // Crashed-plugin observer. Notified once per plugin crash, before events
  // are dispatched to individual plugin instances.
  NPAPIPluginCrashed(subject, topic, data) {
    let propertyBag = subject;
    if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
        !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
        !propertyBag.hasKey("runID") ||
        !propertyBag.hasKey("pluginName")) {
      Cu.reportError("A NPAPI plugin crashed, but the properties of this plugin " +
                     "cannot be read.");
      return;
    }

    let runID = propertyBag.getPropertyAsUint32("runID");
    let uglyPluginName = propertyBag.getPropertyAsAString("pluginName");
    let pluginName = BrowserUtils.makeNicePluginName(uglyPluginName);
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");

    // If we don't have a minidumpID, we can't (or didn't) submit anything.
    // This can happen if the plugin is killed from the task manager.
    let state;
    if (!AppConstants.MOZ_CRASHREPORTER || !gCrashReporter.enabled) {
      // This state tells the user that crash reporting is disabled, so we
      // cannot send a report.
      state = "noSubmit";
    } else if (!pluginDumpID) {
      // This state tells the user that there is no crash report available.
      state = "noReport";
    } else {
      // This state asks the user to submit a crash report.
      state = "please";
    }

    let mm = window.getGroupMessageManager("browsers");
    mm.broadcastAsyncMessage("BrowserPlugins:NPAPIPluginProcessCrashed",
                             { pluginName, runID, state });
  },

  /**
   * Shows a plugin-crashed notification bar for a browser that has had an
   * invisiable NPAPI plugin crash, or a GMP plugin crash.
   *
   * @param browser
   *        The browser to show the notification for.
   * @param messageString
   *        The string to put in the notification bar
   * @param pluginID
   *        The unique-per-process identifier for the NPAPI plugin or GMP.
   *        For a GMP, this is the pluginID. For NPAPI plugins (where "pluginID"
   *        means something different), this is the runID.
   */
  showPluginCrashedNotification(browser, messageString, pluginID) {
    // If there's already an existing notification bar, don't do anything.
    let notificationBox = gBrowser.getNotificationBox(browser);
    let notification = notificationBox.getNotificationWithValue("plugin-crashed");
    if (notification) {
      return;
    }

    // Configure the notification bar
    let priority = notificationBox.PRIORITY_WARNING_MEDIUM;
    let iconURL = "chrome://global/skin/plugins/pluginGeneric.svg";
    let reloadLabel = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.label");
    let reloadKey   = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.accesskey");

    let buttons = [{
      label: reloadLabel,
      accessKey: reloadKey,
      popup: null,
      callback() { browser.reload(); },
    }];

    if (AppConstants.MOZ_CRASHREPORTER &&
        PluginCrashReporter.hasCrashReport(pluginID)) {
      let submitLabel = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.label");
      let submitKey   = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.accesskey");
      let submitButton = {
        label: submitLabel,
        accessKey: submitKey,
        popup: null,
        callback: () => {
          PluginCrashReporter.submitCrashReport(pluginID);
        },
      };

      buttons.push(submitButton);
    }

    notification = notificationBox.appendNotification(messageString, "plugin-crashed",
                                                      iconURL, priority, buttons);

    // Add the "learn more" link.
    let link = notification.ownerDocument.createXULElement("label");
    link.className = "text-link";
    link.setAttribute("value", gNavigatorBundle.getString("crashedpluginsMessage.learnMore"));
    let crashurl = formatURL("app.support.baseURL", true);
    crashurl += "plugin-crashed-notificationbar";
    link.href = crashurl;
    notification.messageText.appendChild(link);
  },
};

gPluginHandler.init();
