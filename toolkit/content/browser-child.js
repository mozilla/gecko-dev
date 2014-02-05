/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/RemoteAddonsChild.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

let SyncHandler = {
  init: function() {
    sendAsyncMessage("SetSyncHandler", {}, {handler: this});
  },

  getFocusedElementAndWindow: function() {
    let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

    let focusedWindow = {};
    let elt = fm.getFocusedElementForWindow(content, true, focusedWindow);
    return [elt, focusedWindow.value];
  },
};

SyncHandler.init();

let WebProgressListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);
  },

  _requestSpec: function (aRequest) {
    if (!aRequest || !(aRequest instanceof Ci.nsIChannel))
      return null;
    return aRequest.QueryInterface(Ci.nsIChannel).URI.spec;
  },

  _setupJSON: function setupJSON(aWebProgress, aRequest) {
    return {
      isTopLevel: aWebProgress.isTopLevel,
      isLoadingDocument: aWebProgress.isLoadingDocument,
      requestURI: this._requestSpec(aRequest),
      loadType: aWebProgress.loadType
    };
  },

  _setupObjects: function setupObjects(aWebProgress) {
    return {
      contentWindow: content,
      // DOMWindow is not necessarily the content-window with subframes.
      DOMWindow: aWebProgress.DOMWindow
    };
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.stateFlags = aStateFlags;
    json.status = aStatus;

    sendAsyncMessage("Content:StateChange", json, objects);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.location = aLocationURI ? aLocationURI.spec : "";
    json.flags = aFlags;

    if (json.isTopLevel) {
      json.canGoBack = docShell.canGoBack;
      json.canGoForward = docShell.canGoForward;
      json.documentURI = content.document.documentURIObject.spec;
      json.charset = content.document.characterSet;
    }

    sendAsyncMessage("Content:LocationChange", json, objects);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.status = aStatus;
    json.message = aMessage;

    sendAsyncMessage("Content:StatusChange", json, objects);
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    let json = this._setupJSON(aWebProgress, aRequest);
    let objects = this._setupObjects(aWebProgress);

    json.state = aState;
    json.status = SecurityUI.getSSLStatusAsString();

    sendAsyncMessage("Content:SecurityChange", json, objects);
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

WebProgressListener.init();

let WebNavigation =  {
  _webNavigation: docShell.QueryInterface(Ci.nsIWebNavigation),

  init: function() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);

    // Send a CPOW for the sessionHistory object.
    let history = this._webNavigation.sessionHistory;
    sendAsyncMessage("WebNavigation:setHistory", {}, {history: history});
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "WebNavigation:GoBack":
        this.goBack();
        break;
      case "WebNavigation:GoForward":
        this.goForward();
        break;
      case "WebNavigation:GotoIndex":
        this.gotoIndex(message.data.index);
        break;
      case "WebNavigation:LoadURI":
        this.loadURI(message.data.uri, message.data.flags);
        break;
      case "WebNavigation:Reload":
        this.reload(message.data.flags);
        break;
      case "WebNavigation:Stop":
        this.stop(message.data.flags);
        break;
    }
  },

  goBack: function() {
    if (this._webNavigation.canGoBack)
      this._webNavigation.goBack();
  },

  goForward: function() {
    if (this._webNavigation.canGoForward)
      this._webNavigation.goForward();
  },

  gotoIndex: function(index) {
    this._webNavigation.gotoIndex(index);
  },

  loadURI: function(uri, flags) {
    this._webNavigation.loadURI(uri, flags, null, null, null);
  },

  reload: function(flags) {
    this._webNavigation.reload(flags);
  },

  stop: function(flags) {
    this._webNavigation.stop(flags);
  }
};

WebNavigation.init();

let SecurityUI = {
  getSSLStatusAsString: function() {
    let status = docShell.securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    if (status) {
      let helper = Cc["@mozilla.org/network/serialization-helper;1"]
                      .getService(Ci.nsISerializationHelper);

      status.QueryInterface(Ci.nsISerializable);
      return helper.serializeToString(status);
    }

    return null;
  }
};

let ControllerCommands = {
  init: function () {
    addMessageListener("ControllerCommands:Do", this);
  },

  receiveMessage: function(message) {
    switch(message.name) {
      case "ControllerCommands:Do":
        if (docShell.isCommandEnabled(message.data))
          docShell.doCommand(message.data);
        break;
    }
  }
}

ControllerCommands.init()

addEventListener("DOMTitleChanged", function (aEvent) {
  let document = content.document;
  switch (aEvent.type) {
  case "DOMTitleChanged":
    if (!aEvent.isTrusted || aEvent.target.defaultView != content)
      return;

    sendAsyncMessage("DOMTitleChanged", { title: document.title });
    break;
  }
}, false);

addEventListener("ImageContentLoaded", function (aEvent) {
  if (content.document instanceof Ci.nsIImageDocument) {
    let req = content.document.imageRequest;
    if (!req.image)
      return;
    sendAsyncMessage("ImageDocumentLoaded", { width: req.image.width,
                                              height: req.image.height });
  }
}, false);

let DocumentObserver = {
  init: function() {
    Services.obs.addObserver(this, "document-element-inserted", false);
    addEventListener("unload", () => {
      Services.obs.removeObserver(this, "document-element-inserted");
    });
  },

  observe: function(aSubject, aTopic, aData) {
    if (aSubject == content.document) {
      sendAsyncMessage("DocumentInserted", {synthetic: aSubject.mozSyntheticDocument});
    }
  },
};
DocumentObserver.init();

function _getMarkupViewer() {
  return docShell.contentViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);
}

addMessageListener("FullZoom", function (aMessage) {
  _getMarkupViewer().fullZoom = aMessage.data.value;
});

addMessageListener("TextZoom", function (aMessage) {
  _getMarkupViewer().textZoom = aMessage.data.value;
});

addEventListener("FullZoomChange", function (aEvent) {
  sendAsyncMessage("FullZoomChange", { value: _getMarkupViewer().fullZoom });
}, false);

addEventListener("TextZoomChange", function (aEvent) {
  sendAsyncMessage("TextZoomChange", { value: _getMarkupViewer().textZoom });
}, false);

RemoteAddonsChild.init(this);

addMessageListener("History:UseGlobalHistory", function (aMessage) {
  docShell.useGlobalHistory = aMessage.data.enabled;
});

let AutoCompletePopup = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup]),

  init: function() {
    // Hook up the form fill autocomplete controller.
    let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                       .getService(Ci.nsIFormFillController);

    controller.attachToBrowser(docShell, this.QueryInterface(Ci.nsIAutoCompletePopup));

    this._input = null;
    this._popupOpen = false;

    addMessageListener("FormAutoComplete:HandleEnter", message => {
      this.selectedIndex = message.data.selectedIndex;

      let controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                  getService(Components.interfaces.nsIAutoCompleteController);
      controller.handleEnter(message.data.isPopupSelection);
    });
  },

  get input () { return this._input; },
  get overrideValue () { return null; },
  set selectedIndex (index) { },
  get selectedIndex () {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    return sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
  },
  get popupOpen () {
    return this._popupOpen;
  },

  openAutocompletePopup: function (input, element) {
    this._input = input;
    this._popupOpen = true;
  },

  closePopup: function () {
    this._popupOpen = false;
    sendAsyncMessage("FormAutoComplete:ClosePopup", {});
  },

  invalidate: function () {
  },

  selectBy: function(reverse, page) {
    this._index = sendSyncMessage("FormAutoComplete:SelectBy", {
      reverse: reverse,
      page: page
    });
  }
}

addMessageListener("FormAutoComplete:InitPopup", function (aMessage) {
  setTimeout(function() AutoCompletePopup.init(), 0);
});
