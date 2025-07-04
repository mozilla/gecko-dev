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
 * Tests that the ip protection main panel view has the correct content.
 */
add_task(async function test_main_content() {
  let button = document.getElementById(lazy.IPProtectionWidget.WIDGET_ID);
  let panelView = PanelMultiView.getViewNode(
    document,
    lazy.IPProtectionWidget.PANEL_ID
  );

  let panelShownPromise = waitForPanelEvent(document, "popupshown");
  // Open the panel
  button.click();
  await panelShownPromise;

  let content = panelView.querySelector(lazy.IPProtectionPanel.CONTENT_TAGNAME);
  Assert.ok(
    BrowserTestUtils.isVisible(content),
    "ipprotection content component should be present"
  );
  Assert.ok(content.upgradeEl, "Upgrade vpn element should be present");
  Assert.ok(
    content.upgradeEl.querySelector("#upgrade-vpn-title"),
    "Upgrade vpn title should be present"
  );
  Assert.ok(
    content.upgradeEl.querySelector("#upgrade-vpn-paragraph"),
    "Upgrade vpn paragraph should be present"
  );
  Assert.ok(
    content.upgradeEl.querySelector("#upgrade-vpn-button"),
    "Upgrade vpn button should be present"
  );

  // Close the panel
  let panelHiddenPromise = waitForPanelEvent(document, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await panelHiddenPromise;
});
