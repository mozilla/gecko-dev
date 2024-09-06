/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const PAGE =
  "https://example.com/browser/remote/shared/listeners/test/browser/test-page-with-css.html";

add_task(async function test_only_for_observed_context() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=test"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await loadURL(tab.linkedBrowser, PAGE);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [PAGE, tab.linkedBrowser.browsingContext],
    async (url, browsingContext) => {
      const { CachedResourceListener } = ChromeUtils.importESModule(
        "chrome://remote/content/shared/listeners/CachedResourceListener.sys.mjs"
      );

      const events = [];
      const onEvent = (name, data) => events.push(data);

      const listener = new CachedResourceListener(browsingContext);
      listener.on("cached-resource-sent", onEvent);
      listener.startListening();

      const head = content.document.getElementsByTagName("HEAD")[0];
      const link = content.document.createElement("link");
      link.rel = "stylesheet";
      link.type = "text/css";
      link.href = "style.css";
      head.appendChild(link);

      await ContentTaskUtils.waitForCondition(() => events.length == 1);

      Assert.equal(
        events.length,
        1,
        "An event for the cached stylesheet is received"
      );

      const iframe = content.document.createElement("iframe");
      iframe.src = url;
      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");

      Assert.equal(events.length, 1, "No new events are received");

      listener.stopListening();
      listener.destroy();
    }
  );

  gBrowser.removeTab(tab);
});
