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
 * Tests that clicking toolbar button opens the panel,
 * and the panel contains a `<ipprotection-content>` element.
 */
add_task(async function click_toolbar_button() {
  let button = document.getElementById(lazy.IPProtectionWidget.WIDGET_ID);
  let panelView = PanelMultiView.getViewNode(
    document,
    lazy.IPProtectionWidget.PANEL_ID
  );

  let panelShownPromise = waitForPanelEvent(document, "popupshown");
  // Open the panel
  button.click();
  await panelShownPromise;

  let header = panelView.querySelector(lazy.IPProtectionPanel.HEADER_TAGNAME);
  Assert.ok(
    BrowserTestUtils.isVisible(header),
    "ipprotection-header component should be present"
  );

  let component = panelView.querySelector(
    lazy.IPProtectionPanel.CONTENT_TAGNAME
  );
  Assert.ok(
    BrowserTestUtils.isVisible(component),
    "ipprotection-content component should be present"
  );

  // Close the panel
  let panelHiddenPromise = waitForPanelEvent(document, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await panelHiddenPromise;
});
