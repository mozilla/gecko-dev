/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BLOCKED_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org:8000/browser/browser/base/content/test/about/csp_iframe.sjs";

add_task(async function test_csp() {
  let iFramePage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example.com"
    ) + "iframe_page_csp.html";

  // Opening the page that contains the iframe
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browser = tab.linkedBrowser;
  let browserLoaded = BrowserTestUtils.browserLoaded(
    browser,
    true,
    BLOCKED_PAGE,
    true
  );

  BrowserTestUtils.startLoadingURIString(browser, iFramePage);
  await browserLoaded;
  info("The error page has loaded!");

  await SpecialPowers.spawn(browser, [], async function () {
    let iframe = content.document.getElementById("theIframe");

    await ContentTaskUtils.waitForCondition(() =>
      SpecialPowers.spawn(iframe, [], () =>
        content.document.body.classList.contains("neterror")
      )
    );
  });

  let iframe = browser.browsingContext.children[0];

  // In the iframe, we see the correct error page
  await SpecialPowers.spawn(iframe, [], async function () {
    let doc = content.document;

    // aboutNetError.mjs is using async localization to format several
    // messages and in result the translation may be applied later.
    // We want to return the textContent of the element only after
    // the translation completes, so let's wait for it here.
    let elements = [doc.getElementById("errorLongDesc")];
    await ContentTaskUtils.waitForCondition(() => {
      return elements.every(elem => !!elem.textContent.trim().length);
    });

    let textLongDescription = doc.getElementById("errorLongDesc").textContent;
    let learnMoreLinkLocation = doc.getElementById("learnMoreLink").href;

    Assert.ok(
      textLongDescription.includes(
        "To see this page, you need to open it in a new window."
      ),
      "Correct error message found"
    );

    Assert.ok(
      learnMoreLinkLocation.includes("xframe-neterror-page"),
      "Correct Learn More URL for CSP error page"
    );
  });

  BrowserTestUtils.removeTab(tab);
});
