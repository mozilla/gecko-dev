/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const CACHED_RESOURCE_URL =
  "https://example.com/browser/remote/shared/listeners/test/browser/cached_resource.sjs";
const STYLESHEET_URL = `${CACHED_RESOURCE_URL}?type=stylesheet`;
const SCRIPT_URL = `${CACHED_RESOURCE_URL}?type=script`;
const IMAGE_URL = `${CACHED_RESOURCE_URL}?type=image`;

add_task(async function test_stylesheet() {
  for (const type of ["stylesheet", "script", "image"]) {
    // Clear the cache.
    Services.cache2.clear();

    const tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      `https://example.com/document-builder.sjs?html=cached_${type}_testpage`
    ));
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    const topContext = tab.linkedBrowser.browsingContext;

    // Setup the cached resource listener and load a resource, no event should
    // be received.
    await setupCachedListener(topContext);
    await loadCachedResource(topContext, type);
    let cachedEventCount = await getCachedResourceEventCount(topContext);
    is(
      cachedEventCount,
      0,
      `[${type}] No cached event received for the initial load`
    );

    // Destroy listener before reloading.
    await destroyCachedListener(topContext);

    // Reload, prepare the cached resource listener and load the same resource
    // again.
    await BrowserTestUtils.reloadTab(tab);
    await setupCachedListener(topContext);
    await loadCachedResource(topContext, type);

    cachedEventCount = await getCachedResourceEventCount(topContext);
    (type === "script" ? todo_is : is)(
      cachedEventCount,
      1,
      `[${type}] 1 cached event received for the second load`
    );

    const iframeContext = await createIframeContext(topContext);
    await setupCachedListener(iframeContext);
    await loadCachedResource(iframeContext, type);

    cachedEventCount = await getCachedResourceEventCount(topContext);
    (type === "script" ? todo_is : is)(
      cachedEventCount,
      1,
      `[${type}] No new event for the top context`
    );

    let iframeCachedEventCount =
      await getCachedResourceEventCount(iframeContext);

    if (type === "image") {
      // For images, loading an image already cached in the parent document
      // will not trigger a fetch. We will have to perform additional requests.
      is(
        iframeCachedEventCount,
        0,
        `[${type}] No event received for the frame context`
      );

      // Load the image with a url suffix to avoid reusing the cached version
      // from the top page.
      await loadCachedResource(iframeContext, type, { addIframeSuffix: true });
      cachedEventCount = await getCachedResourceEventCount(topContext);
      is(cachedEventCount, 1, `[${type}] No new event for the top context`);
      iframeCachedEventCount = await getCachedResourceEventCount(iframeContext);
      is(
        iframeCachedEventCount,
        0,
        `[${type}] Still no event for the frame context`
      );

      // Perform another load of the image in the iframe, this time an event
      // should be emitted for the frame context.
      await loadCachedResource(iframeContext, type, { addIframeSuffix: true });
      cachedEventCount = await getCachedResourceEventCount(topContext);
      is(cachedEventCount, 1, `[${type}] No new event for the top context`);
      iframeCachedEventCount = await getCachedResourceEventCount(iframeContext);
      is(
        iframeCachedEventCount,
        1,
        `[${type}] 1 event received for the frame context`
      );
    } else {
      (type === "script" ? todo_is : is)(
        iframeCachedEventCount,
        1,
        `[${type}] 1 event received for the frame context`
      );
    }

    // Destroy listeners.
    await destroyCachedListener(topContext);
    await destroyCachedListener(iframeContext);

    gBrowser.removeTab(tab);
  }
});

async function loadCachedResource(browsingContext, type, options = {}) {
  info(`Load ${type} for browsingContext ${browsingContext.id}`);

  const { addIframeSuffix = false } = options;
  const getResourceUrl = url => (addIframeSuffix ? url + "&for-iframe" : url);

  switch (type) {
    case "stylesheet": {
      await SpecialPowers.spawn(
        browsingContext,
        [getResourceUrl(STYLESHEET_URL)],
        async url => {
          const head = content.document.getElementsByTagName("HEAD")[0];
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.type = "text/css";
          link.href = url;
          head.appendChild(link);

          info("Wait until the stylesheet has been loaded and applied");
          await ContentTaskUtils.waitForCondition(
            () =>
              content.getComputedStyle(content.document.body)[
                "background-color"
              ] == "rgb(0, 0, 0)"
          );
        }
      );
      break;
    }
    case "script": {
      await SpecialPowers.spawn(
        browsingContext,
        [getResourceUrl(SCRIPT_URL)],
        async url => {
          const script = content.document.createElement("script");
          script.type = "text/javascript";
          script.src = url;
          content.document.body.appendChild(script);

          info("Wait until the script has been loaded and applied");
          await ContentTaskUtils.waitForCondition(
            () => content.wrappedJSObject.scriptLoaded
          );
        }
      );
      break;
    }
    case "image": {
      await SpecialPowers.spawn(
        browsingContext,
        [getResourceUrl(IMAGE_URL)],
        async url => {
          const img = content.document.createElement("img");
          const loaded = new Promise(r => {
            img.addEventListener("load", r, { once: true });
          });
          img.src = url;

          content.document.body.appendChild(img);

          info("Wait until the image has been loaded");
          await loaded;
        }
      );
    }
  }
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
