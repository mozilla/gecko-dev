"use strict";

/**
 * Bug 1933428 - Ensure that we don't hit the invalid first-party cookie
 * assertion when loading a channel which sets a cookie in sandboxed contexts.
 */

const TEST_SITE = "https://example.com/";
const TEST_SANDBOXED_PAGE = TEST_SITE + TEST_PATH + "sandboxedWithImg.html";
const TEST_IMAGE_URL = TEST_SITE + TEST_PATH + "setCookieImg.jpg";

/**
 * Observes the channel for the given URL and checks if it is in the third-party
 * context and verifies the partition key.
 *
 * @param {string} url - The URL to observe.
 * @param {boolean} isThirdParty - Whether the request is in the third-party context.
 * @param {boolean} isPartitionKeyOpaque - Whether the partition key is using an opaque origin.
 * @param {string} partitionKey - The partition key to check for.
 * @returns {Promise} Resolves when the channel is observed.
 */
function observeChannel(url, isThirdParty, isPartitionKeyOpaque, partitionKey) {
  return new Promise(resolve => {
    let observer = {
      observe(aSubject, aTopic) {
        // Make sure that the topic is 'http-on-modify-request'.
        if (aTopic === "http-on-modify-request") {
          let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
          // Make sure this is the request we're looking for.
          if (httpChannel.URI.spec !== url) {
            return;
          }

          let loadInfo = httpChannel.loadInfo;

          if (isPartitionKeyOpaque) {
            ok(
              loadInfo.cookieJarSettings.partitionKey.endsWith(".mozilla"),
              "The partition key is using an opaque origin"
            );
          } else {
            is(
              loadInfo.cookieJarSettings.partitionKey,
              partitionKey,
              "The partition key is correct"
            );
          }
          is(
            loadInfo.isInThirdPartyContext,
            isThirdParty,
            `The request ${
              isThirdParty ? "is" : "is not"
            } in the third-party context`
          );

          Services.obs.removeObserver(observer, "http-on-modify-request");
          resolve();
        }
      },
    };

    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

add_setup(async function () {
  registerCleanupFunction(() => {
    Services.cookies.removeAll();
  });
});

add_task(async function test_top_level_sandboxed_context() {
  // Start observing the channel before opening the tab. The image should be
  // loaded in the first-party context.
  let obsChannelPromise = observeChannel(TEST_IMAGE_URL, false, true);

  info("Opening the top-level sandboxed page");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_SANDBOXED_PAGE
  );

  await obsChannelPromise;

  ok(true, "The loading of the image was successful.");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_sandboxed_iframe() {
  info("Opening the top-level page");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_SITE);

  // Start observing the channel before opening the iframe. The image should be
  // loaded in the third-party context and the partition key should have the
  // foreign ancestor bit set.
  let obsChannelPromise = observeChannel(
    TEST_IMAGE_URL,
    true,
    false,
    "(https,example.com,f)"
  );

  info(" Loading the sandboxed iframe");
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_SANDBOXED_PAGE],
    async src => {
      let iframe = content.document.createElement("iframe");

      await new Promise(resolve => {
        iframe.onload = resolve;
        iframe.src = src;
        content.document.body.appendChild(iframe);
      });
      return iframe.browsingContext;
    }
  );

  await obsChannelPromise;

  ok(true, "The loading of the image in the sandboxed iframe was successful.");

  BrowserTestUtils.removeTab(tab);
});
