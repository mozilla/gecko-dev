/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  IPProtectionWidget: "resource:///modules/ipprotection/IPProtection.sys.mjs",
});

/**
 * Tests that toolbar widget is added and removed based on
 * `browser.ipProtection.enabled`.
 */
add_task(async function toolbar_added_and_removed() {
  let widget = document.getElementById(lazy.IPProtectionWidget.WIDGET_ID);
  ok(
    BrowserTestUtils.isVisible(widget),
    "IP Protection widget should be added to the navbar"
  );
  let position = CustomizableUI.getPlacementOfWidget(
    lazy.IPProtectionWidget.WIDGET_ID
  ).position;
  is(position, 7, "IP Protection widget added in the correct position");
  // Disable the feature
  Services.prefs.clearUserPref("browser.ipProtection.enabled");
  widget = document.getElementById(lazy.IPProtectionWidget.WIDGET_ID);
  is(widget, null, "IP Protection widget is removed");

  // Reenable the feature
  Services.prefs.setBoolPref("browser.ipProtection.enabled", true);
  widget = document.getElementById(lazy.IPProtectionWidget.WIDGET_ID);
  ok(
    BrowserTestUtils.isVisible(widget),
    "IP Protection widget should be added back to the navbar"
  );
});
