/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Social", "CreateSocialStatusWidget",
                         "CreateSocialMarkWidget", "OpenGraphBuilder",
                         "DynamicResizeWatcher", "sizeSocialPanelToContent"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

// The minimum sizes for the auto-resize panel code, minimum size necessary to
// properly show the error page in the panel.
const PANEL_MIN_HEIGHT = 190;
const PANEL_MIN_WIDTH = 330;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SocialService",
  "resource://gre/modules/SocialService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageMetadata",
  "resource://gre/modules/PageMetadata.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");


function promiseSetAnnotation(aURI, providerList) {
  let deferred = Promise.defer();

  // Delaying to catch issues with asynchronous behavior while waiting
  // to implement asynchronous annotations in bug 699844.
  Services.tm.mainThread.dispatch(function() {
    try {
      if (providerList && providerList.length > 0) {
        PlacesUtils.annotations.setPageAnnotation(
          aURI, "social/mark", JSON.stringify(providerList), 0,
          PlacesUtils.annotations.EXPIRE_WITH_HISTORY);
      } else {
        PlacesUtils.annotations.removePageAnnotation(aURI, "social/mark");
      }
    } catch(e) {
      Cu.reportError("SocialAnnotation failed: " + e);
    }
    deferred.resolve();
  }, Ci.nsIThread.DISPATCH_NORMAL);

  return deferred.promise;
}

function promiseGetAnnotation(aURI) {
  let deferred = Promise.defer();

  // Delaying to catch issues with asynchronous behavior while waiting
  // to implement asynchronous annotations in bug 699844.
  Services.tm.mainThread.dispatch(function() {
    let val = null;
    try {
      val = PlacesUtils.annotations.getPageAnnotation(aURI, "social/mark");
    } catch (ex) { }

    deferred.resolve(val);
  }, Ci.nsIThread.DISPATCH_NORMAL);

  return deferred.promise;
}

this.Social = {
  initialized: false,
  lastEventReceived: 0,
  providers: [],
  _disabledForSafeMode: false,

  init: function Social_init() {
    this._disabledForSafeMode = Services.appinfo.inSafeMode && this.enabled;
    let deferred = Promise.defer();

    if (this.initialized) {
      deferred.resolve(true);
      return deferred.promise;
    }
    this.initialized = true;
    // if SocialService.hasEnabledProviders, retreive the providers so the
    // front-end can generate UI
    if (SocialService.hasEnabledProviders) {
      // Retrieve the current set of providers, and set the current provider.
      SocialService.getOrderedProviderList(function (providers) {
        Social._updateProviderCache(providers);
        Social._updateWorkerState(SocialService.enabled);
        deferred.resolve(false);
      });
    } else {
      deferred.resolve(false);
    }

    // Register an observer for changes to the provider list
    SocialService.registerProviderListener(function providerListener(topic, origin, providers) {
      // An engine change caused by adding/removing a provider should notify.
      // any providers we receive are enabled in the AddonsManager
      if (topic == "provider-installed" || topic == "provider-uninstalled") {
        // installed/uninstalled do not send the providers param
        Services.obs.notifyObservers(null, "social:" + topic, origin);
        return;
      }
      if (topic == "provider-enabled") {
        Social._updateProviderCache(providers);
        Social._updateWorkerState(true);
        Services.obs.notifyObservers(null, "social:" + topic, origin);
        return;
      }
      if (topic == "provider-disabled") {
        // a provider was removed from the list of providers, that does not
        // affect worker state for other providers
        Social._updateProviderCache(providers);
        Social._updateWorkerState(providers.length > 0);
        Services.obs.notifyObservers(null, "social:" + topic, origin);
        return;
      }
      if (topic == "provider-update") {
        // a provider has self-updated its manifest, we need to update our cache
        // and reload the provider.
        Social._updateProviderCache(providers);
        let provider = Social._getProviderFromOrigin(origin);
        provider.reload();
      }
    });
    return deferred.promise;
  },

  _updateWorkerState: function(enable) {
    [p.enabled = enable for (p of Social.providers) if (p.enabled != enable)];
  },

  // Called to update our cache of providers and set the current provider
  _updateProviderCache: function (providers) {
    this.providers = providers;
    Services.obs.notifyObservers(null, "social:providers-changed", null);
  },

  get enabled() {
    return !this._disabledForSafeMode && this.providers.length > 0;
  },

  toggleNotifications: function SocialNotifications_toggle() {
    let prefValue = Services.prefs.getBoolPref("social.toast-notifications.enabled");
    Services.prefs.setBoolPref("social.toast-notifications.enabled", !prefValue);
  },

  _getProviderFromOrigin: function (origin) {
    for (let p of this.providers) {
      if (p.origin == origin) {
        return p;
      }
    }
    return null;
  },

  getManifestByOrigin: function(origin) {
    return SocialService.getManifestByOrigin(origin);
  },

  installProvider: function(data, installCallback, options={}) {
    SocialService.installProvider(data, installCallback, options);
  },

  uninstallProvider: function(origin, aCallback) {
    SocialService.uninstallProvider(origin, aCallback);
  },

  // Activation functionality
  activateFromOrigin: function (origin, callback) {
    // It's OK if the provider has already been activated - we still get called
    // back with it.
    SocialService.enableProvider(origin, callback);
  },

  // Page Marking functionality
  isURIMarked: function(origin, aURI, aCallback) {
    promiseGetAnnotation(aURI).then(function(val) {
      if (val) {
        let providerList = JSON.parse(val);
        val = providerList.indexOf(origin) >= 0;
      }
      aCallback(!!val);
    }).then(null, Cu.reportError);
  },

  markURI: function(origin, aURI, aCallback) {
    // update or set our annotation
    promiseGetAnnotation(aURI).then(function(val) {

      let providerList = val ? JSON.parse(val) : [];
      let marked = providerList.indexOf(origin) >= 0;
      if (marked)
        return;
      providerList.push(origin);
      // we allow marking links in a page that may not have been visited yet.
      // make sure there is a history entry for the uri, then annotate it.
      let place = {
        uri: aURI,
        visits: [{
          visitDate: Date.now() + 1000,
          transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
        }]
      };
      PlacesUtils.asyncHistory.updatePlaces(place, {
        handleError: function () Cu.reportError("couldn't update history for socialmark annotation"),
        handleResult: function () {},
        handleCompletion: function () {
          promiseSetAnnotation(aURI, providerList).then(function() {
            if (aCallback)
              schedule(function() { aCallback(true); } );
          }).then(null, Cu.reportError);
        }
      });
    }).then(null, Cu.reportError);
  },

  unmarkURI: function(origin, aURI, aCallback) {
    // this should not be called if this.provider or the port is null
    // set our annotation
    promiseGetAnnotation(aURI).then(function(val) {
      let providerList = val ? JSON.parse(val) : [];
      let marked = providerList.indexOf(origin) >= 0;
      if (marked) {
        // remove the annotation
        providerList.splice(providerList.indexOf(origin), 1);
        promiseSetAnnotation(aURI, providerList).then(function() {
          if (aCallback)
            schedule(function() { aCallback(false); } );
        }).then(null, Cu.reportError);
      }
    }).then(null, Cu.reportError);
  }
};

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

function CreateSocialStatusWidget(aId, aProvider) {
  if (!aProvider.statusURL)
    return;
  let widget = CustomizableUI.getWidget(aId);
  // The widget is only null if we've created then destroyed the widget.
  // Once we've actually called createWidget the provider will be set to
  // PROVIDER_API.
  if (widget && widget.provider == CustomizableUI.PROVIDER_API)
    return;

  CustomizableUI.createWidget({
    id: aId,
    type: "custom",
    removable: true,
    defaultArea: CustomizableUI.AREA_NAVBAR,
    onBuild: function(aDocument) {
      let node = aDocument.createElement("toolbarbutton");
      node.id = this.id;
      node.setAttribute("class", "toolbarbutton-1 chromeclass-toolbar-additional social-status-button badged-button");
      node.style.listStyleImage = "url(" + (aProvider.icon32URL || aProvider.iconURL) + ")";
      node.setAttribute("origin", aProvider.origin);
      node.setAttribute("label", aProvider.name);
      node.setAttribute("tooltiptext", aProvider.name);
      node.setAttribute("oncommand", "SocialStatus.showPopup(this);");
      node.setAttribute("constrain-size", "true");

      if (PrivateBrowsingUtils.isWindowPrivate(aDocument.defaultView))
        node.setAttribute("disabled", "true");

      return node;
    }
  });
};

function CreateSocialMarkWidget(aId, aProvider) {
  if (!aProvider.markURL)
    return;
  let widget = CustomizableUI.getWidget(aId);
  // The widget is only null if we've created then destroyed the widget.
  // Once we've actually called createWidget the provider will be set to
  // PROVIDER_API.
  if (widget && widget.provider == CustomizableUI.PROVIDER_API)
    return;

  CustomizableUI.createWidget({
    id: aId,
    type: "custom",
    removable: true,
    defaultArea: CustomizableUI.AREA_NAVBAR,
    onBuild: function(aDocument) {
      let node = aDocument.createElement("toolbarbutton");
      node.id = this.id;
      node.setAttribute("class", "toolbarbutton-1 chromeclass-toolbar-additional social-mark-button");
      node.setAttribute("type", "socialmark");
      node.setAttribute("constrain-size", "true");
      node.style.listStyleImage = "url(" + (aProvider.unmarkedIcon || aProvider.icon32URL || aProvider.iconURL) + ")";
      node.setAttribute("origin", aProvider.origin);

      let window = aDocument.defaultView;
      let menuLabel = window.gNavigatorBundle.getFormattedString("social.markpageMenu.label", [aProvider.name]);
      node.setAttribute("label", menuLabel);
      node.setAttribute("tooltiptext", menuLabel);
      node.setAttribute("observes", "Social:PageShareOrMark");

      return node;
    }
  });
};


function sizeSocialPanelToContent(panel, iframe, requestedSize) {
  let doc = iframe.contentDocument;
  if (!doc || !doc.body) {
    return;
  }
  // We need an element to use for sizing our panel.  See if the body defines
  // an id for that element, otherwise use the body itself.
  let body = doc.body;
  let docEl = doc.documentElement;
  let bodyId = body.getAttribute("contentid");
  if (bodyId) {
    body = doc.getElementById(bodyId) || doc.body;
  }
  // offsetHeight/Width don't include margins, so account for that.
  let cs = doc.defaultView.getComputedStyle(body);
  let width = Math.max(PANEL_MIN_WIDTH, docEl.offsetWidth);
  let height = Math.max(PANEL_MIN_HEIGHT, docEl.offsetHeight);
  // if the panel is preloaded prior to being shown, cs will be null.  in that
  // case use the minimum size for the panel until it is shown.
  if (cs) {
    let computedHeight = parseInt(cs.marginTop) + body.offsetHeight + parseInt(cs.marginBottom);
    height = Math.max(computedHeight, height);
    let computedWidth = parseInt(cs.marginLeft) + body.offsetWidth + parseInt(cs.marginRight);
    width = Math.max(computedWidth, width);
  }

  // if our scrollHeight is still larger than the iframe, the css calculations
  // above did not work for this site, increase the height. This can happen if
  // the site increases its height for additional UI.
  if (docEl.scrollHeight > iframe.boxObject.height)
    height = docEl.scrollHeight;

  // if a size was defined in the manifest use it as a minimum
  if (requestedSize) {
    if (requestedSize.height)
      height = Math.max(height, requestedSize.height);
    if (requestedSize.width)
      width = Math.max(width, requestedSize.width);
  }

  // add the extra space used by the panel (toolbar, borders, etc) if the iframe
  // has been loaded
  if (iframe.boxObject.width && iframe.boxObject.height) {
    // add extra space the panel needs if any
    width += panel.boxObject.width - iframe.boxObject.width;
    height += panel.boxObject.height - iframe.boxObject.height;
  }

  // using panel.sizeTo will ignore css transitions, set size via style
  if (Math.abs(panel.boxObject.width - width) >= 2)
    panel.style.width = width + "px";
  if (Math.abs(panel.boxObject.height - height) >= 2)
    panel.style.height = height + "px";
}

function DynamicResizeWatcher() {
  this._mutationObserver = null;
}

DynamicResizeWatcher.prototype = {
  start: function DynamicResizeWatcher_start(panel, iframe, requestedSize) {
    this.stop(); // just in case...
    let doc = iframe.contentDocument;
    this._mutationObserver = new iframe.contentWindow.MutationObserver((mutations) => {
      sizeSocialPanelToContent(panel, iframe, requestedSize);
    });
    // Observe anything that causes the size to change.
    let config = {attributes: true, characterData: true, childList: true, subtree: true};
    this._mutationObserver.observe(doc, config);
    // and since this may be setup after the load event has fired we do an
    // initial resize now.
    sizeSocialPanelToContent(panel, iframe, requestedSize);
  },
  stop: function DynamicResizeWatcher_stop() {
    if (this._mutationObserver) {
      try {
        this._mutationObserver.disconnect();
      } catch (ex) {
        // may get "TypeError: can't access dead object" which seems strange,
        // but doesn't seem to indicate a real problem, so ignore it...
      }
      this._mutationObserver = null;
    }
  }
}


this.OpenGraphBuilder = {
  generateEndpointURL: function(URLTemplate, pageData) {
    // support for existing oexchange style endpoints by supporting their
    // querystring arguments. parse the query string template and do
    // replacements where necessary the query names may be different than ours,
    // so we could see u=%{url} or url=%{url}
    let [endpointURL, queryString] = URLTemplate.split("?");
    let query = {};
    if (queryString) {
      queryString.split('&').forEach(function (val) {
        let [name, value] = val.split('=');
        let p = /%\{(.+)\}/.exec(value);
        if (!p) {
          // preserve non-template query vars
          query[name] = value;
        } else if (pageData[p[1]]) {
          if (p[1] == "previews")
            query[name] = pageData[p[1]][0];
          else
            query[name] = pageData[p[1]];
        } else if (p[1] == "body") {
          // build a body for emailers
          let body = "";
          if (pageData.title)
            body += pageData.title + "\n\n";
          if (pageData.description)
            body += pageData.description + "\n\n";
          if (pageData.text)
            body += pageData.text + "\n\n";
          body += pageData.url;
          query["body"] = body;
        }
      });
      // if the url template doesn't have title and no text was provided, add the title as the text.
      if (!query.text && !query.title && pageData.title) {
        query.text = pageData.title;
      }
    }
    var str = [];
    for (let p in query)
       str.push(p + "=" + encodeURIComponent(query[p]));
    if (str.length)
      endpointURL = endpointURL + "?" + str.join("&");
    return endpointURL;
  },
};
