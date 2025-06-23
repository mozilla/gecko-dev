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
 * Tests that clicking toolbar dropmarker opens the panel,
 * and the panel contains a `<ipprotection-panel>` element.
 */
add_task(async function click_toolbar_dropmarker() {
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

  let component = panelView.querySelector(lazy.IPProtectionPanel.TAGNAME);
  ok(
    BrowserTestUtils.isVisible(component),
    "ipprotection-panel component should be present"
  );

  // Close the panel
  let panelHiddenPromise = waitForPanelEvent(document, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await panelHiddenPromise;
});
