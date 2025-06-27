/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  IPProtectionWidget: "resource:///modules/ipprotection/IPProtection.sys.mjs",
  IPProtectionPanel:
    "resource:///modules/ipprotection/IPProtectionPanel.sys.mjs",
});

/**
 * Tests that the ip protection header has the correct content.
 */
add_task(async function test_header_content() {
  let dropmarker = document.getElementById(
    lazy.IPProtectionWidget.WIDGET_ID + "-dropmarker"
  );
  let panelView = PanelMultiView.getViewNode(
    document,
    lazy.IPProtectionWidget.PANEL_ID
  );

  let panelShownPromise = waitForPanelEvent(document, "popupshown");
  // Open the panel
  dropmarker.click();
  await panelShownPromise;

  let header = panelView.querySelector(lazy.IPProtectionPanel.HEADER_TAGNAME);
  Assert.ok(
    BrowserTestUtils.isVisible(header),
    "ipprotection-header component should be present"
  );
  Assert.ok(header.titleEl, "ipprotection-header title should be present");

  // Close the panel
  let panelHiddenPromise = waitForPanelEvent(document, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await panelHiddenPromise;
});
