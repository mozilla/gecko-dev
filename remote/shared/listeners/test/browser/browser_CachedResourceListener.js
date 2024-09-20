/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const STYLESHEET_URL =
  "https://example.com/browser/remote/shared/listeners/test/browser/cached_style.sjs";

add_task(async function test_only_for_observed_context() {
  // Clear the cache.
  Services.cache2.clear();

  const tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=cached_css_testpage"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const topContext = tab.linkedBrowser.browsingContext;

  // Setup the cached resource listener and load a stylesheet in a link tag, no
  // event should be received.
  await setupCachedListener(topContext);
  await loadStylesheet(topContext, STYLESHEET_URL);
  let cachedEventCount = await getCachedResourceEventCount(topContext);
  is(cachedEventCount, 0, "No cached event received for the initial load");

  // Destroy listener before reloading.
  await destroyCachedListener(topContext);

  // Reload, prepare the cached resource listener and load the same stylesheet
  // again.
  await BrowserTestUtils.reloadTab(tab);
  await setupCachedListener(topContext);
  await loadStylesheet(topContext, STYLESHEET_URL);

  cachedEventCount = await getCachedResourceEventCount(topContext);
  is(cachedEventCount, 1, "1 cached event received for the second load");

  const iframeContext = await createIframeContext(topContext);
  await setupCachedListener(iframeContext);
  await loadStylesheet(iframeContext, STYLESHEET_URL);

  cachedEventCount = await getCachedResourceEventCount(topContext);
  is(cachedEventCount, 1, "No new event for the top context");

  let iframeCachedEventCount = await getCachedResourceEventCount(iframeContext);
  is(iframeCachedEventCount, 1, "1 event received for the frame context");

  // Destroy listeners.
  await destroyCachedListener(topContext);
  await destroyCachedListener(iframeContext);

  gBrowser.removeTab(tab);
});

async function loadStylesheet(browsingContext, url) {
  info(`Load stylesheet for browsingContext ${browsingContext.id}`);
  await SpecialPowers.spawn(browsingContext, [url], async _url => {
    const head = content.document.getElementsByTagName("HEAD")[0];
    const link = content.document.createElement("link");
    link.rel = "stylesheet";
    link.type = "text/css";
    link.href = _url;
    head.appendChild(link);

    info("Wait until the stylesheet has been loaded and applied");
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(content.document.body)["background-color"] ==
        "rgb(0, 0, 0)"
    );
  });
}

async function setupCachedListener(browsingContext) {
  info(
    `Setup cachedResourcelistener for browsingContext ${browsingContext.id}`
  );
  await SpecialPowers.spawn(
    browsingContext,
    [browsingContext],
    async _browsingContext => {
      const { CachedResourceListener } = ChromeUtils.importESModule(
        "chrome://remote/content/shared/listeners/CachedResourceListener.sys.mjs"
      );

      content.wrappedJSObject.cachedResourceEvents = [];
      const onEvent = (name, data) => {
        content.wrappedJSObject.cachedResourceEvents.push(data);
      };

      const listener = new CachedResourceListener(_browsingContext);
      listener.on("cached-resource-sent", onEvent);
      listener.startListening();
      content.wrappedJSObject.cachedResourcelistener = listener;
    }
  );
}

async function destroyCachedListener(browsingContext) {
  info(
    `Destroy cachedResourcelistener for browsingContext ${browsingContext.id}`
  );
  return SpecialPowers.spawn(browsingContext, [], async () => {
    // Cleanup the listener.
    content.wrappedJSObject.cachedResourcelistener.stopListening();
    content.wrappedJSObject.cachedResourcelistener.destroy();
  });
}

async function getCachedResourceEventCount(browsingContext) {
  info(
    `Retrieve cached resource events count for browsingContext ${browsingContext.id}`
  );
  return SpecialPowers.spawn(browsingContext, [], async () => {
    return content.wrappedJSObject.cachedResourceEvents.length;
  });
}

async function createIframeContext(browsingContext) {
  info(`Create iframe in browsingContext ${browsingContext.id}`);
  return SpecialPowers.spawn(browsingContext, [], async () => {
    const iframe = content.document.createElement("iframe");
    iframe.src =
      "https://example.com/document-builder.sjs?html=cached_css_frame";
    content.document.body.appendChild(iframe);
    await ContentTaskUtils.waitForEvent(iframe, "load");
    return iframe.browsingContext;
  });
}
