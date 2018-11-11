/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TelProtocolHandle.js
 *
 * This file implements the URLs for Telephone Calls
 * https://www.ietf.org/rfc/rfc2806.txt
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource:///modules/TelURIParser.jsm");
Cu.import('resource://gre/modules/ActivityChannel.jsm');

function TelProtocolHandler() {
}

TelProtocolHandler.prototype = {

  scheme: "tel",
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_NORELATIVE |
                 Ci.nsIProtocolHandler.URI_NOAUTH |
                 Ci.nsIProtocolHandler.URI_LOADABLE_BY_ANYONE |
                 Ci.nsIProtocolHandler.URI_DOES_NOT_RETURN_DATA,
  allowPort: () => false,

  newURI: function Proto_newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel2: function Proto_newChannel(aURI, aLoadInfo) {
    let number = TelURIParser.parseURI('tel', aURI.spec);

    if (number) {
      return new ActivityChannel(aURI, aLoadInfo,
                                 "dial-handler",
                                 { number: number,
                                   type: "webtelephony/number" });
    }

    throw Components.results.NS_ERROR_ILLEGAL_VALUE;
  },

  newChannel: function Proto_newChannel(aURI) {
    return this.newChannel2(aURI, null);
  },

  classID: Components.ID("{782775dd-7351-45ea-aff1-0ffa872cfdd2}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelProtocolHandler]);
