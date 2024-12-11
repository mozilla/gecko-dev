/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextualIdentityService } from "resource://gre/modules/ContextualIdentityService.sys.mjs";

export function QuotaUtilsService() {}

QuotaUtilsService.prototype = {
  getPrivateIdentityId(aName) {
    const privateIdentity = ContextualIdentityService.getPrivateIdentity(aName);
    if (!privateIdentity) {
      return 0;
    }
    return privateIdentity.userContextId;
  },
  QueryInterface: ChromeUtils.generateQI(["nsIQuotaUtilsService"]),
  classDescription: "Quota Utils Service",
  contractID: "@mozilla.org/dom/quota-utils-service;1",
  classID: Components.ID("3e65d9b5-5b41-4c18-ac7b-681b9df9df97"),
};
