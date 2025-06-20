/**
 * @file Defines the environment for remote page.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export default {
  globals: {
    atob: "readonly",
    btoa: "readonly",
    RPMAddTRRExcludedDomain: "readonly",
    RPMGetAppBuildID: "readonly",
    RPMGetInnerMostURI: "readonly",
    RPMGetIntPref: "readonly",
    RPMGetStringPref: "readonly",
    RPMGetBoolPref: "readonly",
    RPMSetPref: "readonly",
    RPMGetFormatURLPref: "readonly",
    RPMIsTRROnlyFailure: "readonly",
    RPMIsFirefox: "readonly",
    RPMIsWindowPrivate: "readonly",
    RPMSendAsyncMessage: "readonly",
    RPMSendQuery: "readonly",
    RPMAddMessageListener: "readonly",
    RPMRecordGleanEvent: "readonly",
    RPMCheckAlternateHostAvailable: "readonly",
    RPMRemoveMessageListener: "readonly",
    RPMGetHttpResponseHeader: "readonly",
    RPMTryPingSecureWWWLink: "readonly",
    RPMOpenSecureWWWLink: "readonly",
    RPMOpenPreferences: "readonly",
    RPMHasConnectivity: "readonly",
    RPMGetTRRSkipReason: "readonly",
    RPMGetTRRDomain: "readonly",
    RPMIsSiteSpecificTRRError: "readonly",
    RPMSetTRRDisabledLoadFlags: "readonly",
    RPMShowOSXLocalNetworkPermissionWarning: "readonly",
  },
};
