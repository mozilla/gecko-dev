/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PluginChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/BrowserUtils.jsm");

ChromeUtils.defineModuleGetter(this, "ContextMenuChild",
                               "resource:///actors/ContextMenuChild.jsm");

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  const url = "chrome://browser/locale/browser.properties";
  return Services.strings.createBundle(url);
});

ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");

const OVERLAY_DISPLAY = {
  HIDDEN: 0, // The overlay will be transparent
  BLANK: 1, // The overlay will be just a grey box
  TINY: 2, // The overlay with a 16x16 plugin icon
  REDUCED: 3, // The overlay with a 32x32 plugin icon
  NOTEXT: 4, // The overlay with a 48x48 plugin icon and the close button
  FULL: 5, // The full overlay: 48x48 plugin icon, close button and label
};

class PluginChild extends ActorChild {
  constructor(dispatcher) {
    super(dispatcher);

    // Cache of plugin actions for the current page.
    this.pluginData = new Map();
    // Cache of plugin crash information sent from the parent
    this.pluginCrashData = new Map();

    this.mm.addEventListener("pagehide", this, {capture: true, mozSystemGroup: true});
    this.mm.addEventListener("pageshow", this, {capture: true, mozSystemGroup: true});
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "BrowserPlugins:ActivatePlugins":
        this.activatePlugins(msg.data.pluginInfo, msg.data.newState);
        break;
      case "BrowserPlugins:ContextMenuCommand":
        switch (msg.data.command) {
          case "play":
            this._showClickToPlayNotification(ContextMenuChild.getTarget(this.mm, msg, "plugin"), true);
            break;
          case "hide":
            this.hideClickToPlayOverlay(ContextMenuChild.getTarget(this.mm, msg, "plugin"));
            break;
        }
        break;
      case "BrowserPlugins:NPAPIPluginProcessCrashed":
        this.NPAPIPluginProcessCrashed({
          pluginName: msg.data.pluginName,
          runID: msg.data.runID,
          state: msg.data.state,
        });
        break;
      case "BrowserPlugins:CrashReportSubmitted":
        this.NPAPIPluginCrashReportSubmitted({
          runID: msg.data.runID,
          state: msg.data.state,
        });
        break;
      case "BrowserPlugins:Test:ClearCrashData":
        // This message should ONLY ever be sent by automated tests.
        if (Services.prefs.getBoolPref("plugins.testmode")) {
          this.pluginCrashData.clear();
        }
    }
  }

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "decoder-doctor-notification":
        let data = JSON.parse(aData);
        let type = data.type.toLowerCase();
        if (type == "cannot-play" &&
            this.haveShownNotification &&
            aSubject.top.document == this.content.document &&
            data.formats.toLowerCase().includes("application/x-mpegurl", 0)) {
          this.content.pluginRequiresReload = true;
        }
    }
  }

  onPageShow(event) {
    // Ignore events that aren't from the main document.
    if (!this.content || event.target != this.content.document) {
      return;
    }

    // The PluginClickToPlay events are not fired when navigating using the
    // BF cache. |persisted| is true when the page is loaded from the
    // BF cache, so this code reshows the notification if necessary.
    if (event.persisted) {
      this.reshowClickToPlayNotification();
    }
  }

  onPageHide(event) {
    // Ignore events that aren't from the main document.
    if (!this.content || event.target != this.content.document) {
      return;
    }

    this.clearPluginCaches();
    this.haveShownNotification = false;
  }

  getPluginUI(plugin, anonid) {
    if (plugin.openOrClosedShadowRoot &&
        plugin.openOrClosedShadowRoot.isUAWidget()) {
      return plugin.openOrClosedShadowRoot.getElementById(anonid);
    }
    return plugin.ownerDocument.
      getAnonymousElementByAttribute(plugin, "anonid", anonid);
  }

  _getPluginInfo(pluginElement) {
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    pluginElement.QueryInterface(Ci.nsIObjectLoadingContent);

    let tagMimetype;
    let pluginName = gNavigatorBundle.GetStringFromName("pluginInfo.unknownPlugin");
    let pluginTag = null;
    let permissionString = null;
    let fallbackType = null;
    let blocklistState = null;

    tagMimetype = pluginElement.actualType;
    if (tagMimetype == "") {
      tagMimetype = pluginElement.type;
    }

    if (this.isKnownPlugin(pluginElement)) {
      pluginTag = pluginHost.getPluginTagForType(pluginElement.actualType);
      pluginName = BrowserUtils.makeNicePluginName(pluginTag.name);

      // Convert this from nsIPluginTag so it can be serialized.
      let properties = ["name", "description", "filename", "version", "enabledState", "niceName"];
      let pluginTagCopy = {};
      for (let prop of properties) {
        pluginTagCopy[prop] = pluginTag[prop];
      }
      pluginTag = pluginTagCopy;

      permissionString = pluginHost.getPermissionStringForType(pluginElement.actualType);
      fallbackType = pluginElement.defaultFallbackType;
      blocklistState = pluginHost.getBlocklistStateForType(pluginElement.actualType);
      // Make state-softblocked == state-notblocked for our purposes,
      // they have the same UI. STATE_OUTDATED should not exist for plugin
      // items, but let's alias it anyway, just in case.
      if (blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED ||
          blocklistState == Ci.nsIBlocklistService.STATE_OUTDATED) {
        blocklistState = Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
      }
    }

    return { mimetype: tagMimetype,
             pluginName,
             pluginTag,
             permissionString,
             fallbackType,
             blocklistState,
           };
  }

  /**
   * _getPluginInfoForTag is called when iterating the plugins for a document,
   * and what we get from nsIDOMWindowUtils is an nsIPluginTag, and not an
   * nsIObjectLoadingContent. This only should happen if the plugin is
   * click-to-play (see bug 1186948).
   */
  _getPluginInfoForTag(pluginTag, tagMimetype) {
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

    let pluginName = gNavigatorBundle.GetStringFromName("pluginInfo.unknownPlugin");
    let permissionString = null;
    let blocklistState = null;

    if (pluginTag) {
      pluginName = BrowserUtils.makeNicePluginName(pluginTag.name);

      permissionString = pluginHost.getPermissionStringForTag(pluginTag);
      blocklistState = pluginTag.blocklistState;

      // Convert this from nsIPluginTag so it can be serialized.
      let properties = ["name", "description", "filename", "version", "enabledState", "niceName"];
      let pluginTagCopy = {};
      for (let prop of properties) {
        pluginTagCopy[prop] = pluginTag[prop];
      }
      pluginTag = pluginTagCopy;

      // Make state-softblocked == state-notblocked for our purposes,
      // they have the same UI. STATE_OUTDATED should not exist for plugin
      // items, but let's alias it anyway, just in case.
      if (blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED ||
          blocklistState == Ci.nsIBlocklistService.STATE_OUTDATED) {
        blocklistState = Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
      }
    }

    return { mimetype: tagMimetype,
             pluginName,
             pluginTag,
             permissionString,
             // Since we should only have entered _getPluginInfoForTag when
             // examining a click-to-play plugin, we can safely hard-code
             // this fallback type, since we don't actually have an
             // nsIObjectLoadingContent to check.
             fallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
             blocklistState,
           };
  }

  /**
   * Update the visibility of the plugin overlay.
   */
  setVisibility(plugin, overlay, overlayDisplayState) {
    overlay.classList.toggle("visible", overlayDisplayState != OVERLAY_DISPLAY.HIDDEN);
    if (overlayDisplayState != OVERLAY_DISPLAY.HIDDEN) {
      overlay.removeAttribute("dismissed");
    }
  }

  /**
   * Adjust the style in which the overlay will be displayed. It might be adjusted
   * based on its size, or if there's some other element covering all corners of
   * the overlay.
   *
   * This function will handle adjusting the style of the overlay, but will
   * not handle hiding it. That is done by setVisibility with the return value
   * from this function.
   *
   * @returns A value from OVERLAY_DISPLAY.
   */
  computeAndAdjustOverlayDisplay(plugin, overlay) {
    let fallbackType = plugin.pluginFallbackType;
    if (plugin.pluginFallbackTypeOverride !== undefined) {
      fallbackType = plugin.pluginFallbackTypeOverride;
    }
    if (fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET) {
      return OVERLAY_DISPLAY.HIDDEN;
    }

    // If the overlay size is 0, we haven't done layout yet. Presume that
    // plugins are visible until we know otherwise.
    if (overlay.scrollWidth == 0) {
      return OVERLAY_DISPLAY.FULL;
    }

    let overlayDisplay = OVERLAY_DISPLAY.FULL;

    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    let pluginWidth = Math.ceil(pluginRect.width);
    let pluginHeight = Math.ceil(pluginRect.height);

    // We must set the attributes while here inside this function in order
    // for a possible re-style to occur, which will make the scrollWidth/Height
    // checks below correct. Otherwise, we would be requesting e.g. a TINY
    // overlay here, but the default styling would be used, and that would make
    // it overflow, causing it to change to BLANK instead of remaining as TINY.

    if (pluginWidth <= 32 || pluginHeight <= 32) {
      overlay.setAttribute("sizing", "blank");
      overlayDisplay = OVERLAY_DISPLAY.BLANK;
    } else if (pluginWidth <= 80 || pluginHeight <= 60) {
      overlayDisplay = OVERLAY_DISPLAY.TINY;
      overlay.setAttribute("sizing", "tiny");
      overlay.setAttribute("notext", "notext");
    } else if (pluginWidth <= 120 || pluginHeight <= 80) {
      overlayDisplay = OVERLAY_DISPLAY.REDUCED;
      overlay.setAttribute("sizing", "reduced");
      overlay.setAttribute("notext", "notext");
    } else if (pluginWidth <= 240 || pluginHeight <= 160) {
      overlayDisplay = OVERLAY_DISPLAY.NOTEXT;
      overlay.removeAttribute("sizing");
      overlay.setAttribute("notext", "notext");
    } else {
      overlayDisplay = OVERLAY_DISPLAY.FULL;
      overlay.removeAttribute("sizing");
      overlay.removeAttribute("notext");
    }


    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > pluginWidth) ||
                    (overlay.scrollHeight - 5 > pluginHeight);
    if (overflows) {
      overlay.setAttribute("sizing", "blank");
      return OVERLAY_DISPLAY.BLANK;
    }

    // Is the plugin covered up by other content so that it is not clickable?
    // Floating point can confuse .elementFromPoint, so inset just a bit
    let left = pluginRect.left + 2;
    let right = pluginRect.right - 2;
    let top = pluginRect.top + 2;
    let bottom = pluginRect.bottom - 2;
    let centerX = left + (right - left) / 2;
    let centerY = top + (bottom - top) / 2;
    let points = [[left, top],
                   [left, bottom],
                   [right, top],
                   [right, bottom],
                   [centerX, centerY]];

    let contentWindow = plugin.ownerGlobal;
    let cwu = contentWindow.windowUtils;

    for (let [x, y] of points) {
      if (x < 0 || y < 0) {
        continue;
      }
      let el = cwu.elementFromPoint(x, y, true, true);
      if (el === plugin) {
        return overlayDisplay;
      }
    }

    overlay.setAttribute("sizing", "blank");
    return OVERLAY_DISPLAY.BLANK;
  }

  addLinkClickCallback(linkNode, callbackName /* callbackArgs...*/) {
    // XXX just doing (callback)(arg) was giving a same-origin error. bug?
    let self = this;
    let callbackArgs = Array.prototype.slice.call(arguments).slice(2);
    linkNode.addEventListener("click",
                              function(evt) {
                                if (!evt.isTrusted)
                                  return;
                                evt.preventDefault();
                                if (callbackArgs.length == 0)
                                  callbackArgs = [ evt ];
                                (self[callbackName]).apply(self, callbackArgs);
                              },
                              true);

    linkNode.addEventListener("keydown",
                              function(evt) {
                                if (!evt.isTrusted)
                                  return;
                                if (evt.keyCode == evt.DOM_VK_RETURN) {
                                  evt.preventDefault();
                                  if (callbackArgs.length == 0)
                                    callbackArgs = [ evt ];
                                  evt.preventDefault();
                                  (self[callbackName]).apply(self, callbackArgs);
                                }
                              },
                              true);
  }

  // Helper to get the binding handler type from a plugin object
  _getBindingType(plugin) {
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return null;

    switch (plugin.pluginFallbackType) {
      case Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED:
        return "PluginNotFound";
      case Ci.nsIObjectLoadingContent.PLUGIN_DISABLED:
        return "PluginDisabled";
      case Ci.nsIObjectLoadingContent.PLUGIN_BLOCKLISTED:
        return "PluginBlocklisted";
      case Ci.nsIObjectLoadingContent.PLUGIN_OUTDATED:
        return "PluginOutdated";
      case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY:
      case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET:
        return "PluginClickToPlay";
      case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
        return "PluginVulnerableUpdatable";
      case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
        return "PluginVulnerableNoUpdate";
      default:
        // Not all states map to a handler
        return null;
    }
  }

  handleEvent(event) {
    let eventType = event.type;

    if (eventType == "pagehide") {
      this.onPageHide(event);
      return;
    }

    if (eventType == "pageshow") {
      this.onPageShow(event);
      return;
    }

    if (eventType == "click") {
      this.onOverlayClick(event);
      return;
    }

    if (eventType == "PluginCrashed" &&
        !(event.target instanceof Ci.nsIObjectLoadingContent)) {
      // If the event target is not a plugin object (i.e., an <object> or
      // <embed> element), this call is for a window-global plugin.
      this.onPluginCrashed(event.target, event);
      return;
    }

    if (eventType == "HiddenPlugin") {
      let pluginTag = event.tag.QueryInterface(Ci.nsIPluginTag);
      if (event.target.defaultView.top.document != this.content.document) {
        return;
      }
      this._showClickToPlayNotification(pluginTag, false);
    }

    let plugin = event.target;

    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return;

    if (eventType == "PluginBindingAttached") {
      // The plugin binding fires this event when it is created.
      // As an untrusted event, ensure that this object actually has a binding
      // and make sure we don't handle it twice
      let overlay = this.getPluginUI(plugin, "main");
      if (!overlay || overlay._bindingHandled) {
        return;
      }
      overlay._bindingHandled = true;

      // Lookup the handler for this binding
      eventType = this._getBindingType(plugin);
      if (!eventType) {
        // Not all bindings have handlers
        return;
      }
    }

    let shouldShowNotification = false;
    switch (eventType) {
      case "PluginCrashed":
        this.onPluginCrashed(plugin, event);
        break;

      case "PluginNotFound": {
        /* NOP */
        break;
      }

      case "PluginBlocklisted":
      case "PluginOutdated":
        shouldShowNotification = true;
        break;

      case "PluginVulnerableUpdatable":
        let updateLink = this.getPluginUI(plugin, "checkForUpdatesLink");
        let { pluginTag } = this._getPluginInfo(plugin);
        this.addLinkClickCallback(updateLink, "forwardCallback",
                                  "openPluginUpdatePage", pluginTag);
        /* FALLTHRU */

      case "PluginVulnerableNoUpdate":
      case "PluginClickToPlay":
        this._handleClickToPlayEvent(plugin);
        let pluginName = this._getPluginInfo(plugin).pluginName;
        let messageString = gNavigatorBundle.formatStringFromName("PluginClickToActivate2", [pluginName], 1);
        let overlayText = this.getPluginUI(plugin, "clickToPlay");
        overlayText.textContent = messageString;
        if (eventType == "PluginVulnerableUpdatable" ||
            eventType == "PluginVulnerableNoUpdate") {
          let vulnerabilityString = gNavigatorBundle.GetStringFromName(eventType);
          let vulnerabilityText = this.getPluginUI(plugin, "vulnerabilityStatus");
          vulnerabilityText.textContent = vulnerabilityString;
        }
        shouldShowNotification = true;
        break;

      case "PluginDisabled":
        let manageLink = this.getPluginUI(plugin, "managePluginsLink");
        this.addLinkClickCallback(manageLink, "forwardCallback", "managePlugins");
        shouldShowNotification = true;
        break;

      case "PluginInstantiated":
        shouldShowNotification = true;
        break;
    }

    // Show the in-content UI if it's not too big. The crashed plugin handler already did this.
    let overlay = this.getPluginUI(plugin, "main");
    if (eventType != "PluginCrashed") {
      if (overlay != null) {
        this.setVisibility(plugin, overlay,
                           this.computeAndAdjustOverlayDisplay(plugin, overlay));
        let resizeListener = () => {
          this.setVisibility(plugin, overlay,
            this.computeAndAdjustOverlayDisplay(plugin, overlay));
        };
        plugin.addEventListener("overflow", resizeListener);
        plugin.addEventListener("underflow", resizeListener);
      }
    }

    let closeIcon = this.getPluginUI(plugin, "closeIcon");
    if (closeIcon) {
      closeIcon.addEventListener("click", clickEvent => {
        if (clickEvent.button == 0 && clickEvent.isTrusted) {
          this.hideClickToPlayOverlay(plugin);
          overlay.setAttribute("dismissed", "true");
        }
      }, true);
    }

    if (shouldShowNotification) {
      this._showClickToPlayNotification(plugin, false);
    }
  }

  isKnownPlugin(objLoadingContent) {
    return (objLoadingContent.getContentTypeForMIMEType(objLoadingContent.actualType) ==
            Ci.nsIObjectLoadingContent.TYPE_PLUGIN);
  }

  canActivatePlugin(objLoadingContent) {
    // if this isn't a known plugin, we can't activate it
    // (this also guards pluginHost.getPermissionStringForType against
    // unexpected input)
    if (!this.isKnownPlugin(objLoadingContent))
      return false;

    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    let principal = objLoadingContent.ownerGlobal.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(principal, permissionString);

    let isFallbackTypeValid =
      objLoadingContent.pluginFallbackType >= Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY &&
      objLoadingContent.pluginFallbackType <= Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET;

    return !objLoadingContent.activated &&
           pluginPermission != Ci.nsIPermissionManager.DENY_ACTION &&
           isFallbackTypeValid;
  }

  hideClickToPlayOverlay(plugin) {
    let overlay = this.getPluginUI(plugin, "main");
    if (overlay) {
      overlay.classList.remove("visible");
    }
  }

  // Forward a link click callback to the chrome process.
  forwardCallback(name, pluginTag) {
    this.mm.sendAsyncMessage("PluginContent:LinkClickCallback",
      { name, pluginTag });
  }

  submitReport(plugin) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }
    if (!plugin) {
      Cu.reportError("Attempted to submit crash report without an associated plugin.");
      return;
    }
    if (!(plugin instanceof Ci.nsIObjectLoadingContent)) {
      Cu.reportError("Attempted to submit crash report on plugin that does not" +
                     "implement nsIObjectLoadingContent.");
      return;
    }

    let runID = plugin.runID;
    let submitURLOptIn = this.getPluginUI(plugin, "submitURLOptIn").checked;
    let keyVals = {};
    let userComment = this.getPluginUI(plugin, "submitComment").value.trim();
    if (userComment)
      keyVals.PluginUserComment = userComment;
    if (submitURLOptIn)
      keyVals.PluginContentURL = plugin.ownerDocument.URL;

    this.mm.sendAsyncMessage("PluginContent:SubmitReport",
                                 { runID, keyVals, submitURLOptIn });
  }

  reloadPage() {
    this.content.location.reload();
  }

  // Event listener for click-to-play plugins.
  _handleClickToPlayEvent(plugin) {
    let doc = plugin.ownerDocument;
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    // guard against giving pluginHost.getPermissionStringForType a type
    // not associated with any known plugin
    if (!this.isKnownPlugin(objLoadingContent))
      return;
    let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    let principal = doc.defaultView.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(principal, permissionString);

    let overlay = this.getPluginUI(plugin, "main");

    if (pluginPermission == Ci.nsIPermissionManager.DENY_ACTION ||
        pluginPermission == Ci.nsIObjectLoadingContent.PLUGIN_PERMISSION_PROMPT_ACTION_QUIET) {
      if (overlay) {
        overlay.classList.remove("visible");
      }
      return;
    }

    if (overlay) {
      overlay.addEventListener("click", this, true);
    }
  }

  onOverlayClick(event) {
    let document = event.target.ownerDocument;
    let plugin = document.getBindingParent(event.target);
    let overlay = this.getPluginUI(plugin, "main");
    // Have to check that the target is not the link to update the plugin
    if (!(ChromeUtils.getClassName(event.originalTarget) === "HTMLAnchorElement") &&
        event.originalTarget.getAttribute("anonid") != "closeIcon" &&
        event.originalTarget.id != "closeIcon" &&
        !overlay.hasAttribute("dismissed") &&
        event.button == 0 &&
        event.isTrusted) {
      this._showClickToPlayNotification(plugin, true);
    event.stopPropagation();
    event.preventDefault();
    }
  }

  reshowClickToPlayNotification() {
    let contentWindow = this.content;
    let cwu = contentWindow.windowUtils;
    let plugins = cwu.plugins;
    for (let plugin of plugins) {
      let overlay = this.getPluginUI(plugin, "main");
      if (overlay)
        overlay.removeEventListener("click", this, true);
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (this.canActivatePlugin(objLoadingContent))
        this._handleClickToPlayEvent(plugin);
    }
    this._showClickToPlayNotification(null, false);
  }

  /**
   * Activate the plugins that the user has specified.
   */
  activatePlugins(pluginInfo, newState) {
    let contentWindow = this.content;
    let cwu = contentWindow.windowUtils;
    let plugins = cwu.plugins;
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

    let pluginFound = false;
    for (let plugin of plugins) {
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (!this.isKnownPlugin(plugin)) {
        continue;
      }
      if (pluginInfo.permissionString == pluginHost.getPermissionStringForType(plugin.actualType)) {
        let overlay = this.getPluginUI(plugin, "main");
        pluginFound = true;
        if (newState == "block" || newState == "blockalways" || newState == "continueblocking") {
          if (overlay) {
            overlay.addEventListener("click", this, true);
          }
          plugin.pluginFallbackTypeOverride = pluginInfo.fallbackType;
          plugin.reload(true);
        } else if (this.canActivatePlugin(plugin)) {
          if (overlay) {
            overlay.removeEventListener("click", this, true);
          }
          plugin.playPlugin();
        }
      }
    }

    // If there are no instances of the plugin on the page any more or if we've
    // noted that the content needs to be reloaded due to replacing HLS, what the
    // user probably needs is for us to allow and then refresh.
    if (newState != "block" && newState != "blockalways" && newState != "continueblocking" &&
       (!pluginFound || contentWindow.pluginRequiresReload)) {
      this.reloadPage();
    }
  }

  _showClickToPlayNotification(plugin, showNow) {
    let plugins = [];

    // If plugin is null, that means the user has navigated back to a page with
    // plugins, and we need to collect all the plugins.
    if (plugin === null) {
      let contentWindow = this.content;
      let cwu = contentWindow.windowUtils;
      // cwu.plugins may contain non-plugin <object>s, filter them out
      plugins = cwu.plugins.filter((p) =>
        p.getContentTypeForMIMEType(p.actualType) == Ci.nsIObjectLoadingContent.TYPE_PLUGIN);

      if (plugins.length == 0) {
        this.removeNotification("click-to-play-plugins");
        return;
      }
    } else {
      plugins = [plugin];
    }

    let pluginData = this.pluginData;

    let principal = this.content.document.nodePrincipal;
    let location = this.content.document.location.href;

    for (let p of plugins) {
      let pluginInfo;
      if (p instanceof Ci.nsIPluginTag) {
        let mimeType = p.getMimeTypes() > 0 ? p.getMimeTypes()[0] : null;
        pluginInfo = this._getPluginInfoForTag(p, mimeType);
      } else {
        pluginInfo = this._getPluginInfo(p);
      }
      if (pluginInfo.permissionString === null) {
        Cu.reportError("No permission string for active plugin.");
        continue;
      }
      if (pluginData.has(pluginInfo.permissionString)) {
        continue;
      }

      let permissionObj = Services.perms.
        getPermissionObject(principal, pluginInfo.permissionString, false);
      if (permissionObj) {
        pluginInfo.pluginPermissionPrePath = permissionObj.principal.originNoSuffix;
        pluginInfo.pluginPermissionType = permissionObj.expireType;
      } else {
        pluginInfo.pluginPermissionPrePath = principal.originNoSuffix;
        pluginInfo.pluginPermissionType = undefined;
      }

      this.pluginData.set(pluginInfo.permissionString, pluginInfo);
    }

    this.haveShownNotification = true;

    this.mm.sendAsyncMessage("PluginContent:ShowClickToPlayNotification", {
      plugins: [...this.pluginData.values()],
      showNow,
      location,
    }, null, principal);
  }

  removeNotification(name) {
    this.mm.sendAsyncMessage("PluginContent:RemoveNotification", { name });
  }

  clearPluginCaches() {
    this.pluginData.clear();
    this.pluginCrashData.clear();
  }

  /**
   * Determines whether or not the crashed plugin is contained within current
   * full screen DOM element.
   * @param fullScreenElement (DOM element)
   *   The DOM element that is currently full screen, or null.
   * @param domElement
   *   The DOM element which contains the crashed plugin, or the crashed plugin
   *   itself.
   * @returns bool
   *   True if the plugin is a descendant of the full screen DOM element, false otherwise.
   **/
  isWithinFullScreenElement(fullScreenElement, domElement) {

    /**
     * Traverses down iframes until it find a non-iframe full screen DOM element.
     * @param fullScreenIframe
     *  Target iframe to begin searching from.
     * @returns DOM element
     *  The full screen DOM element contained within the iframe (could be inner iframe), or the original iframe if no inner DOM element is found.
     **/
    let getTrueFullScreenElement = fullScreenIframe => {
      if (typeof fullScreenIframe.contentDocument !== "undefined" && fullScreenIframe.contentDocument.mozFullScreenElement) {
        return getTrueFullScreenElement(fullScreenIframe.contentDocument.mozFullScreenElement);
      }
      return fullScreenIframe;
    };

    if (fullScreenElement.tagName === "IFRAME") {
      fullScreenElement = getTrueFullScreenElement(fullScreenElement);
    }

    if (fullScreenElement.contains(domElement)) {
      return true;
    }
    let parentIframe = domElement.ownerGlobal.frameElement;
    if (parentIframe) {
      return this.isWithinFullScreenElement(fullScreenElement, parentIframe);
    }
    return false;
  }

  /**
   * The PluginCrashed event handler. Note that the PluginCrashed event is
   * fired for both NPAPI and Gecko Media plugins. In the latter case, the
   * target of the event is the document that the GMP is being used in.
   */
  onPluginCrashed(target, aEvent) {
    if (!(aEvent instanceof this.content.PluginCrashedEvent))
      return;

    let fullScreenElement = this.content.document.mozFullScreenElement;
    if (fullScreenElement) {
      if (this.isWithinFullScreenElement(fullScreenElement, target)) {
        this.content.document.mozCancelFullScreen();
      }
    }

    if (aEvent.gmpPlugin) {
      this.GMPCrashed(aEvent);
      return;
    }

    if (!(target instanceof Ci.nsIObjectLoadingContent))
      return;

    let crashData = this.pluginCrashData.get(target.runID);
    if (!crashData) {
      // We haven't received information from the parent yet about
      // this crash, so we should hold off showing the crash report
      // UI.
      return;
    }

    crashData.instances.delete(target);
    if (crashData.instances.length == 0) {
      this.pluginCrashData.delete(target.runID);
    }

    this.setCrashedNPAPIPluginState({
      plugin: target,
      state: crashData.state,
      message: crashData.message,
    });
  }

  NPAPIPluginProcessCrashed({pluginName, runID, state}) {
    let message =
      gNavigatorBundle.formatStringFromName("crashedpluginsMessage.title",
                                            [pluginName], 1);

    let contentWindow = this.content;
    let cwu = contentWindow.windowUtils;
    let plugins = cwu.plugins;

    for (let plugin of plugins) {
      if (plugin instanceof Ci.nsIObjectLoadingContent &&
          plugin.runID == runID) {
        // The parent has told us that the plugin process has died.
        // It's possible that this content process hasn't yet noticed,
        // in which case we need to stash this data around until the
        // PluginCrashed events get sent up.
        if (plugin.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_CRASHED) {
          // This plugin has already been put into the crashed state by the
          // content process, so we can tweak its crash UI without delay.
          this.setCrashedNPAPIPluginState({plugin, state, message});
        } else {
          // The content process hasn't yet determined that the plugin has crashed.
          // Stash the data in our map, and throw the plugin into a WeakSet. When
          // the PluginCrashed event fires on the <object>/<embed>, we'll retrieve
          // the information we need from the Map and remove the instance from the
          // WeakSet. Once the WeakSet is empty, we can clear the map.
          if (!this.pluginCrashData.has(runID)) {
            this.pluginCrashData.set(runID, {
              state,
              message,
              instances: new WeakSet(),
            });
          }
          let crashData = this.pluginCrashData.get(runID);
          crashData.instances.add(plugin);
        }
      }
    }
  }

  setCrashedNPAPIPluginState({plugin, state, message}) {
    // Force a layout flush so the binding is attached.
    plugin.clientTop;
    let overlay = this.getPluginUI(plugin, "main");
    let statusDiv = this.getPluginUI(plugin, "submitStatus");
    let optInCB = this.getPluginUI(plugin, "submitURLOptIn");

    this.getPluginUI(plugin, "submitButton")
        .addEventListener("click", (event) => {
          if (event.button != 0 || !event.isTrusted)
            return;
          this.submitReport(plugin);
        });

    let pref = Services.prefs.getBranch("dom.ipc.plugins.reportCrashURL");
    optInCB.checked = pref.getBoolPref("");

    statusDiv.setAttribute("status", state);

    let helpIcon = this.getPluginUI(plugin, "helpIcon");
    this.addLinkClickCallback(helpIcon, "openHelpPage");

    let crashText = this.getPluginUI(plugin, "crashedText");
    crashText.textContent = message;

    let link = this.getPluginUI(plugin, "reloadLink");
    this.addLinkClickCallback(link, "reloadPage");

    let overlayDisplayState = this.computeAndAdjustOverlayDisplay(plugin, overlay);

    // Is the <object>'s size too small to hold what we want to show?
    if (overlayDisplayState != OVERLAY_DISPLAY.FULL) {
      // First try hiding the crash report submission UI.
      statusDiv.removeAttribute("status");

      overlayDisplayState = this.computeAndAdjustOverlayDisplay(plugin, overlay);
    }
    this.setVisibility(plugin, overlay, overlayDisplayState);

    let doc = plugin.ownerDocument;
    let runID = plugin.runID;

    if (overlayDisplayState == OVERLAY_DISPLAY.FULL) {
      doc.mozNoPluginCrashedNotification = true;

      // Notify others that the crash reporter UI is now ready.
      // Currently, this event is only used by tests.
      let winUtils = this.content.windowUtils;
      let event = new this.content.CustomEvent("PluginCrashReporterDisplayed", {bubbles: true});
      winUtils.dispatchEventToChromeOnly(plugin, event);
    } else if (!doc.mozNoPluginCrashedNotification) {
      // If another plugin on the page was large enough to show our UI, we don't
      // want to show a notification bar.
      this.mm.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification",
                                   { messageString: message, pluginID: runID });
    }
  }

  NPAPIPluginCrashReportSubmitted({ runID, state }) {
    this.pluginCrashData.delete(runID);
    let contentWindow = this.content;
    let cwu = contentWindow.windowUtils;
    let plugins = cwu.plugins;

    for (let plugin of plugins) {
      if (plugin instanceof Ci.nsIObjectLoadingContent &&
          plugin.runID == runID) {
        let statusDiv = this.getPluginUI(plugin, "submitStatus");
        statusDiv.setAttribute("status", state);
      }
    }
  }

  GMPCrashed(aEvent) {
    let target          = aEvent.target;
    let pluginName      = aEvent.pluginName;
    let gmpPlugin       = aEvent.gmpPlugin;
    let pluginID        = aEvent.pluginID;
    let doc             = target.document;

    if (!gmpPlugin || !doc) {
      // TODO: Throw exception? How did we get here?
      return;
    }

    let messageString =
      gNavigatorBundle.formatStringFromName("crashedpluginsMessage.title",
                                            [pluginName], 1);

    this.mm.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification",
                                 { messageString, pluginID });

    // Remove the notification when the page is reloaded.
    doc.defaultView.top.addEventListener("unload", event => {
      this.hideNotificationBar("plugin-crashed");
    });
  }
}
