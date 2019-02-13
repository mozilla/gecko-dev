/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/osfile.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

// This component is used for handling dragover and drop of urls.
//
// It checks to see whether a drop of a url is allowed. For instance, a url
// cannot be dropped if it is not a valid uri or the source of the drag cannot
// access the uri. This prevents, for example, a source document from tricking
// the user into dragging a chrome url.

function ContentAreaDropListener() { };

ContentAreaDropListener.prototype =
{
  classID:          Components.ID("{1f34bc80-1bc7-11d6-a384-d705dd0746fc}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDroppedLinkHandler, Ci.nsISupports]),

  _getDropURL : function (dt)
  {
    let types = dt.types;
    for (let t = 0; t < types.length; t++) {
      let type = types[t];
      switch (type) {
        case "text/uri-list":
          var url = dt.getData("URL").replace(/^\s+|\s+$/g, "");
          return [url, url];
        case "text/plain":
        case "text/x-moz-text-internal":
          var url = dt.getData(type).replace(/^\s+|\s+$/g, "");
          return [url, url];
        case "text/x-moz-url":
          return dt.getData(type).split("\n");
      }
    }

    // For shortcuts, we want to check for the file type last, so that the
    // url pointed to in one of the url types is found first before the file
    // type, which points to the actual file.
    let files = dt.files;
    if (files && files.length) {
      return [OS.Path.toFileURI(files[0].mozFullPath), files[0].name];
    }

    return [ ];
  },

  _validateURI: function(dataTransfer, uriString, disallowInherit)
  {
    if (!uriString)
      return "";

    // Strip leading and trailing whitespace, then try to create a
    // URI from the dropped string. If that succeeds, we're
    // dropping a URI and we need to do a security check to make
    // sure the source document can load the dropped URI.
    uriString = uriString.replace(/^\s*|\s*$/g, '');

    let uri;
    let ioService = Cc["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
    try {
      // Check that the uri is valid first and return an empty string if not.
      // It may just be plain text and should be ignored here
      uri = ioService.newURI(uriString, null, null);
    } catch (ex) { }
    if (!uri)
      return uriString;

    // uriString is a valid URI, so do the security check.
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].
                   getService(Ci.nsIScriptSecurityManager);
    let sourceNode = dataTransfer.mozSourceNode;
    let flags = secMan.STANDARD;
    if (disallowInherit)
      flags |= secMan.DISALLOW_INHERIT_PRINCIPAL;

    // Use file:/// as the default uri so that drops of file URIs are always allowed
    let principal = sourceNode ? sourceNode.nodePrincipal
                               : secMan.getSimpleCodebasePrincipal(ioService.newURI("file:///", null, null));

    secMan.checkLoadURIStrWithPrincipal(principal, uriString, flags);

    return uriString;
  },

  canDropLink: function(aEvent, aAllowSameDocument)
  {
    if (this._eventTargetIsDisabled(aEvent))
      return false;

    let dataTransfer = aEvent.dataTransfer;
    let types = dataTransfer.types;
    if (!types.contains("application/x-moz-file") &&
        !types.contains("text/x-moz-url") &&
        !types.contains("text/uri-list") &&
        !types.contains("text/x-moz-text-internal") &&
        !types.contains("text/plain"))
      return false;

    if (aAllowSameDocument)
      return true;

    let sourceNode = dataTransfer.mozSourceNode;
    if (!sourceNode)
      return true;

    // don't allow a drop of a node from the same document onto this one
    let sourceDocument = sourceNode.ownerDocument;
    let eventDocument = aEvent.originalTarget.ownerDocument;
    if (sourceDocument == eventDocument)
      return false;

    // also check for nodes in other child or sibling frames by checking
    // if both have the same top window.
    if (sourceDocument && eventDocument) {
      let sourceRoot = sourceDocument.defaultView.top;
      if (sourceRoot && sourceRoot == eventDocument.defaultView.top)
        return false;
    }

    return true;
  },

  dropLink: function(aEvent, aName, aDisallowInherit)
  {
    aName.value = "";
    if (this._eventTargetIsDisabled(aEvent))
      return "";

    let dataTransfer = aEvent.dataTransfer;
    let [url, name] = this._getDropURL(dataTransfer);

    try {
      url = this._validateURI(dataTransfer, url, aDisallowInherit);
    } catch (ex) {
      aEvent.stopPropagation();
      aEvent.preventDefault();
      throw ex;
    }

    if (name)
      aName.value = name;

    return url;
  },

  _eventTargetIsDisabled: function(aEvent)
  {
    let ownerDoc = aEvent.originalTarget.ownerDocument;
    if (!ownerDoc || !ownerDoc.defaultView)
      return false;

    return ownerDoc.defaultView
                   .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIDOMWindowUtils)
                   .isNodeDisabledForEvents(aEvent.originalTarget);
  }
};

var components = [ContentAreaDropListener];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
