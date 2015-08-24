/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {interfaces: Ci, utils: Cu, results: Cr, manager: Cm} = Components;
const PAGE_URI = "http://mochi.test:8888/tests/dom/telephony/test/mochitest/test_telephony_messenger.html";
const MANIFEST_URI = "http://mochi.test:8888/manifest.webapp";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gTelephonyMessenger",
                                   "@mozilla.org/ril/system-messenger-helper;1",
                                   "nsITelephonyMessenger");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

addMessageListener("registerPage", () => {
    gSystemMessenger.registerPage("telephony-tty-mode-changed",
      Services.io.newURI(PAGE_URI, null, null),
      Services.io.newURI(MANIFEST_URI, null, null));
    sendAsyncMessage("registerPageCompleted");
  });

addMessageListener("notifyTtyModeChanged", (aMode) => {
    gTelephonyMessenger.notifyTtyModeChanged(aMode);
    sendAsyncMessage("notifyTtyModeChangedCompleted");
  });

