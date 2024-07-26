/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
requestLongerTimeout(3);
const TEST_URL = URL_ROOT + "doc_network-observer.html";
const TEST_URL_CSP = URL_ROOT + "override_script_src_self.html";
const REQUEST_URL =
  URL_ROOT + `sjs_network-observer-test-server.sjs?sts=200&fmt=js`;
const CORS_REQUEST_URL = REQUEST_URL.replace("example.com", "plop.example.com");
const CSP_SCRIPT_TO_OVERRIDE = URL_ROOT + "csp_script_to_override.js";
const GZIPPED_REQUEST_URL = URL_ROOT + `gzipped.sjs`;
const OVERRIDE_FILENAME = "override.js";
const OVERRIDE_HTML_FILENAME = "override.html";

add_task(async function testLocalOverride() {
  await addTab(TEST_URL);

  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel =>
      ![REQUEST_URL, CORS_REQUEST_URL].includes(channel.URI.spec),
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_FILENAME);
  info(" override " + REQUEST_URL + " to " + overrideFile.path + "\n");
  networkObserver.override(REQUEST_URL, overrideFile.path);

  info("Assert that request and cached request are overriden");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async _url => {
      const request = await content.wrappedJSObject.fetch(_url);
      const requestcontent = await request.text();
      is(
        requestcontent,
        `"use strict";\ndocument.title = "Override script loaded";\n`,
        "the request content has been overriden"
      );
      const secondRequest = await content.wrappedJSObject.fetch(_url);
      const secondRequestcontent = await secondRequest.text();
      is(
        secondRequestcontent,
        `"use strict";\ndocument.title = "Override script loaded";\n`,
        "the cached request content has been overriden"
      );
    }
  );

  info("Assert that JS scripts can be overriden");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async _url => {
      const script = content.document.createElement("script");
      const onLoad = new Promise(resolve =>
        script.addEventListener("load", resolve, { once: true })
      );
      script.src = _url;
      content.document.body.appendChild(script);
      await onLoad;
      is(
        content.document.title,
        "Override script loaded",
        "The <script> tag content has been overriden and correctly evaluated"
      );
    }
  );

  info(`Assert that JS scripts with crossorigin="anonymous" can be overriden`);
  networkObserver.override(CORS_REQUEST_URL, overrideFile.path);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [CORS_REQUEST_URL],
    async _url => {
      content.document.title = "title before crossorigin=anonymous evaluation";
      const script = content.document.createElement("script");
      script.setAttribute("crossorigin", "anonymous");
      script.crossOrigin = "anonymous";
      const onLoad = new Promise(resolve =>
        script.addEventListener("load", resolve, { once: true })
      );
      script.src = _url;
      content.document.body.appendChild(script);
      await onLoad;
      is(
        content.document.title,
        "Override script loaded",
        `The <script crossorigin="anonymous"> tag content has been overriden and correctly evaluated`
      );
    }
  );

  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);

  networkObserver.destroy();
});

add_task(async function testHtmlFileOverride() {
  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== TEST_URL,
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_HTML_FILENAME);
  info(" override " + TEST_URL + " to " + overrideFile.path + "\n");
  networkObserver.override(TEST_URL, overrideFile.path);

  await addTab(TEST_URL);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [TEST_URL],
    async pageUrl => {
      is(
        content.document.documentElement.outerHTML,
        "<html><head></head><body>Overriden!\n</body></html>",
        "The content of the HTML has been overriden"
      );
      is(
        content.location.href,
        pageUrl,
        "The location of the page is still the original one"
      );
    }
  );
  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);
  networkObserver.destroy();
});

// Exact same test, but with a gzipped request, which requires very special treatment
add_task(async function testLocalOverrideGzipped() {
  await addTab(TEST_URL);

  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== GZIPPED_REQUEST_URL,
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_FILENAME);
  info(" override " + GZIPPED_REQUEST_URL + " to " + overrideFile.path + "\n");
  networkObserver.override(GZIPPED_REQUEST_URL, overrideFile.path);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [GZIPPED_REQUEST_URL],
    async _url => {
      const request = await content.wrappedJSObject.fetch(_url);
      const requestcontent = await request.text();
      is(
        requestcontent,
        `"use strict";\ndocument.title = "Override script loaded";\n`,
        "the request content for the gzipped script has been overriden"
      );
      const secondRequest = await content.wrappedJSObject.fetch(_url);
      const secondRequestcontent = await secondRequest.text();
      is(
        secondRequestcontent,
        `"use strict";\ndocument.title = "Override script loaded";\n`,
        "the cached request content for the gzipped script has been overriden"
      );
    }
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [GZIPPED_REQUEST_URL],
    async _url => {
      const script = content.document.createElement("script");
      const onLoad = new Promise(resolve =>
        script.addEventListener("load", resolve, { once: true })
      );
      script.src = _url;
      content.document.body.appendChild(script);
      await onLoad;
      is(
        content.document.title,
        "Override script loaded",
        "The <script> tag content for the gzipped script has been overriden and correctly evaluated"
      );
    }
  );

  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);

  networkObserver.destroy();
});

// Check that the override works even if the page uses script 'self' as CSP.
add_task(async function testLocalOverrideCSP() {
  await addTab(TEST_URL_CSP);

  const url = CSP_SCRIPT_TO_OVERRIDE;
  const browser = gBrowser.selectedBrowser;
  const originalText = await getRequestText(url, browser);
  is(
    originalText,
    `"use strict";\ndocument.title = "CSP script to override loaded";\n`,
    "the request content for the CSP script is the original one"
  );

  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== url,
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_FILENAME);
  info(" override " + url + " to " + overrideFile.path + "\n");
  networkObserver.override(url, overrideFile.path);

  const overriddenText = await getRequestText(url, browser);
  is(
    overriddenText,
    `"use strict";\ndocument.title = "Override script loaded";\n`,
    "the request content for the CSP script has been overriden"
  );
  const cachedOverriddenText = await getRequestText(url, browser);
  is(
    cachedOverriddenText,
    `"use strict";\ndocument.title = "Override script loaded";\n`,
    "the cached request content for the CSP script has been overriden"
  );

  await SpecialPowers.spawn(browser, [url], async _url => {
    const script = content.document.createElement("script");
    const onLoad = new Promise(resolve =>
      script.addEventListener("load", resolve, { once: true })
    );
    script.src = _url;
    content.document.body.appendChild(script);
    await onLoad;
    is(
      content.document.title,
      "Override script loaded",
      "The <script> tag content  for the CSP script has been overriden and correctly evaluated"
    );
  });

  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);

  info("Remove the override for " + url);
  networkObserver.removeOverride(url);
  const restoredText = await getRequestText(url, browser);
  is(
    restoredText,
    `"use strict";\ndocument.title = "CSP script to override loaded";\n`,
    "the request content for the CSP script is back to the original one"
  );

  networkObserver.destroy();
});

/**
 * Retrieve the text content for a request to the provided url, as fetched by
 * the provided browser.
 *
 * @param {string} url
 *     The URL of the request to fetch.
 * @param {Browser} browser
 *     The content browser where the request should be fetched.
 * @returns {string}
 *     The text content of the fetch request.
 */
async function getRequestText(url, browser) {
  return SpecialPowers.spawn(browser, [url], async _url => {
    const request = await content.wrappedJSObject.fetch(_url);
    return await request.text();
  });
}
