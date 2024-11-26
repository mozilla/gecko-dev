/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PROXY_TYPE = "network.proxy.type";
const proxyService = Ci.nsIProtocolProxyService;
// The integer pref value for proxy type gets saved to telemetry
// as a string for better readabillity according to the following map
const PROXY_TYPES_MAP_REVERSE = new Map([
  [proxyService.PROXYCONFIG_DIRECT, "DIRECT"],
  [proxyService.PROXYCONFIG_MANUAL, "MANUAL"],
  [proxyService.PROXYCONFIG_PAC, "PAC"],
  [proxyService.PROXYCONFIG_WPAD, "WPAD"],
  [proxyService.PROXYCONFIG_SYSTEM, "SYSTEM"],
]);

add_task(async function testProxyTelemetry() {
  // Open settings dialog
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const connectionURL =
    "chrome://browser/content/preferences/dialogs/connection.xhtml";
  let dialog = await openAndLoadSubDialog(connectionURL);
  let dialogElement = dialog.document.getElementById("ConnectionsDialog");

  // Revert the proxy type to what it was before the test was run
  let oldNetworkProxyType = Services.prefs.getIntPref(PROXY_TYPE);
  registerCleanupFunction(function () {
    Services.prefs.setIntPref(PROXY_TYPE, oldNetworkProxyType);
  });

  let proxyType = dialog.Preferences.get(PROXY_TYPE);

  setPrefAndCheck(
    dialogElement,
    proxyType,
    proxyService.PROXYCONFIG_DIRECT,
    PROXY_TYPES_MAP_REVERSE.get(proxyService.PROXYCONFIG_DIRECT)
  );
  setPrefAndCheck(
    dialogElement,
    proxyType,
    proxyService.PROXYCONFIG_MANUAL,
    PROXY_TYPES_MAP_REVERSE.get(proxyService.PROXYCONFIG_MANUAL)
  );
  setPrefAndCheck(
    dialogElement,
    proxyType,
    proxyService.PROXYCONFIG_PAC,
    PROXY_TYPES_MAP_REVERSE.get(proxyService.PROXYCONFIG_PAC)
  );
  setPrefAndCheck(
    dialogElement,
    proxyType,
    proxyService.PROXYCONFIG_WPAD,
    PROXY_TYPES_MAP_REVERSE.get(proxyService.PROXYCONFIG_WPAD)
  );
  setPrefAndCheck(
    dialogElement,
    proxyType,
    proxyService.PROXYCONFIG_SYSTEM,
    PROXY_TYPES_MAP_REVERSE.get(proxyService.PROXYCONFIG_SYSTEM)
  );
  setPrefAndCheck(dialogElement, proxyType, 42, "OTHER");

  gBrowser.removeCurrentTab();
});

function setPrefAndCheck(
  dialogElement,
  proxyType,
  proxyTypePref,
  proxyTypePrefStr
) {
  Services.fog.testResetFOG();

  proxyType.value = proxyTypePref;
  dialogElement.acceptDialog();
  let events = Glean.networkProxySettings.proxyTypePreference.testGetValue();
  is(
    events[0].extra.value,
    proxyTypePrefStr,
    `Extra field for '${proxyTypePref}' should be '${proxyTypePrefStr}'.`
  );
}
