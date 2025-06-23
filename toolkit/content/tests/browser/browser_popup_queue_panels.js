/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

function createPanel(doc, id, queueable) {
  const panel = doc.createXULElement("panel");
  panel.setAttribute("id", id);

  if (queueable) {
    panel.setAttribute("queue", "true");
  }

  panel.setAttribute("type", "arrow");
  panel.setAttribute("flip", "both");
  panel.setAttribute("consumeoutsideclicks", "false");
  doc.documentElement.appendChild(panel);

  return panel;
}

add_task(async function test_queueable_panels() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    const win = browser.ownerGlobal;
    const doc = win.document;

    const panel1 = createPanel(doc, "panel-one", true);
    const panel2 = createPanel(doc, "panel-two", true);

    panel1.openPopup(null, "topcenter topleft", 100, 100, false, null);

    await BrowserTestUtils.waitForPopupEvent(panel1, "shown");
    info("First panel shown");

    panel2.openPopup(null, "topcenter topleft", 150, 150, false, null);
    await new Promise(r => setTimeout(r, 200));
    is(panel2.state, "closed", "Second panel is still queued");

    panel1.hidePopup();
    await BrowserTestUtils.waitForPopupEvent(panel2, "shown");
    info("Second panel shown after first dismissed");

    panel2.hidePopup();
    await BrowserTestUtils.waitForPopupEvent(panel2, "hidden");

    panel1.remove();
    panel2.remove();
  });
});

add_task(async function test_unqueueable_panel_dismiss_existing_ones() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    const win = browser.ownerGlobal;
    const doc = win.document;

    const panel1 = createPanel(doc, "panel-one", true);
    const panel2 = createPanel(doc, "panel-two", true);
    const panel3 = createPanel(doc, "panel-three", false);

    panel1.openPopup(null, "topcenter topleft", 100, 100, false, null);
    await BrowserTestUtils.waitForPopupEvent(panel1, "shown");
    info("First panel shown");

    panel2.openPopup(null, "topcenter topleft", 150, 150, false, null);
    await new Promise(r => setTimeout(r, 200));
    is(panel2.state, "closed", "Second panel is still queued");

    const hidePanel1 = BrowserTestUtils.waitForPopupEvent(panel1, "hidden");

    panel3.openPopup(null, "topcenter topleft", 200, 200, false, null);
    await BrowserTestUtils.waitForPopupEvent(panel3, "shown");

    await hidePanel1;

    panel3.hidePopup();
    await BrowserTestUtils.waitForPopupEvent(panel2, "shown");
    info("Second panel shown after third dismissed");

    panel2.hidePopup();
    await BrowserTestUtils.waitForPopupEvent(panel2, "hidden");

    panel1.remove();
    panel2.remove();
    panel3.remove();
  });
});

add_task(async function test_queueable_panels_after_unqueueable_ones() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    const win = browser.ownerGlobal;
    const doc = win.document;

    const panel1 = createPanel(doc, "panel-one", false);
    const panel2 = createPanel(doc, "panel-two", true);

    panel1.openPopup(null, "topcenter topleft", 100, 100, false, null);
    await BrowserTestUtils.waitForPopupEvent(panel1, "shown");
    info("First panel shown");

    panel2.openPopup(null, "topcenter topleft", 150, 150, false, null);
    await new Promise(r => setTimeout(r, 200));
    is(panel2.state, "closed", "Second panel is still queued");

    panel1.hidePopup();
    await BrowserTestUtils.waitForPopupEvent(panel2, "shown");
    info("Second panel shown after first dismissed");

    panel2.hidePopup();
    await BrowserTestUtils.waitForPopupEvent(panel2, "hidden");

    panel1.remove();
    panel2.remove();
  });
});
