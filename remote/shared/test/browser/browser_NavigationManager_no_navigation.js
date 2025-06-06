/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NavigationManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/NavigationManager.sys.mjs"
);
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

add_task(async function testDocumentOpenWriteClose() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("fragment-navigated", onEvent);
  navigationManager.on("history-updated", onEvent);
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);
  navigationManager.on("same-document-changed", onEvent);

  const url = "https://example.com/document-builder.sjs?html=test";

  const tab = addTab(gBrowser, url);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  navigationManager.startMonitoring();
  is(events.length, 0, "No event recorded");

  info("Replace the document");
  await SpecialPowers.spawn(browser, [], async () => {
    // Note: we need to use eval here to have reduced permissions and avoid
    // security errors.
    content.eval(`
      document.open();
      document.write("<h1 class='replaced'>Replaced</h1>");
      document.close();
    `);

    await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".replaced")
    );
  });

  is(events.length, 1, "No event recorded after replacing the document");
  is(
    events[0].name,
    "history-updated",
    "Received a single history-updated event"
  );
  is(
    events[0].data.navigationId,
    undefined,
    "history-updated event should not have a navigation id set"
  );
  is(events[0].data.url, url, "history-updated has the expected url");

  info("Reload the page, which should trigger a navigation");
  await loadURL(browser, url);

  info("Wait until 3 events have been received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 3);

  is(events.length, 3, "Recorded 3 navigation events");
  is(
    events[1].name,
    "navigation-started",
    "Received a navigation-started event"
  );
  is(
    events[2].name,
    "navigation-stopped",
    "Received a navigation-stopped event"
  );
  navigationManager.off("fragment-navigated", onEvent);
  navigationManager.off("history-updated", onEvent);
  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.off("same-document-changed", onEvent);

  navigationManager.stopMonitoring();
});
