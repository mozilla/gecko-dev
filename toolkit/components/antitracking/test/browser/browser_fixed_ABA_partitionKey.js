/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test verifies that the partitionKey of an ABA iframe is fixed and won't
 * be changed by loading a cross-site iframe along side it.
 */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior.optInPartitioning", false]],
  });

  registerCleanupFunction(async () => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
        resolve()
      );
    });
  });
});

add_task(async function test_fixed_ABA_partitionKey() {
  info("Open a new tab to load an ABA iframe.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE_HTTPS
  );

  info("Open a cross-site iframe.");
  let crossOriginBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_3RD_PARTY_PAGE],
    async src => {
      // Open an cross-site iframe
      let iframe = content.document.createElement("iframe");
      iframe.src = src;

      await new content.Promise(resolve => {
        iframe.onload = resolve;

        content.document.body.appendChild(iframe);
      });

      return iframe.browsingContext;
    }
  );

  info("Open an ABA iframe and a cross-site iframe in parallel.");
  let abaBC = await SpecialPowers.spawn(
    crossOriginBC,
    [TEST_TOP_PAGE_HTTPS, TEST_3RD_PARTY_PAGE],
    async (sameSiteSrc, crossSiteSrc) => {
      let ABAiframe = content.document.createElement("iframe");
      ABAiframe.src = sameSiteSrc;

      // We don't wait for the ABA iframe to load, because we want to test that
      // loading a cross-site iframe doesn't change the partitionKey of the ABA
      // iframe.
      let abaPromise = new content.Promise(resolve => {
        ABAiframe.onload = resolve;

        content.document.body.appendChild(ABAiframe);
      });

      // Loading a cross-site iframe.
      let crossSiteIframe = content.document.createElement("iframe");
      crossSiteIframe.src = crossSiteSrc;

      let crossSitePromise = new content.Promise(resolve => {
        crossSiteIframe.onload = resolve;
        content.document.body.appendChild(crossSiteIframe);
      });

      // Wait for both iframes to load
      await Promise.all([abaPromise, crossSitePromise]);

      return ABAiframe.browsingContext;
    }
  );

  info("Verify the partitionKey and cookie access of the ABA iframe.");
  await SpecialPowers.spawn(abaBC, [], async _ => {
    is(
      content.document.cookieJarSettings.partitionKey,
      "(https,example.net,f)",
      "ABA iframe has the correct partitionKey"
    );

    content.document.cookie = "test=test";
    is(content.document.cookie, "test=test", "Cookie set");
  });

  BrowserTestUtils.removeTab(tab);
});
