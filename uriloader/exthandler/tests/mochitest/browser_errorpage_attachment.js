"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// Test Context: In bug 1918167, error page loads started unintentionally
// triggering the logic from bug 1756980 and opening in a new tab. It's unclear
// what the best behaviour here is, but we've kept that new behaviour as it
// seems better than an unexpected navigation. If this is broken in the future,
// it may be worth re-evaluating the desired behaviour here.

add_task(async function load_errored_content_disposition_header() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "blank.html", async browser => {
    info("blank loaded, adding link");

    let newTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      TEST_PATH + "emptyErrorPage.sjs?attachment",
      /* waitForLoad */ true,
      /* waitForAnyTab */ true,
      /* maybeErrorPage */ true
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.document.body.innerHTML = `<a href="emptyErrorPage.sjs?attachment">click</a>`;
      let link = content.document.querySelector("a");
      link.click();
    });

    let newTab = await newTabPromise;

    let documentURI = await SpecialPowers.spawn(
      newTab.linkedBrowser,
      [],
      () => content.document.documentURI
    );
    ok(
      documentURI.startsWith("about:neterror"),
      "has loaded neterror page in new tab"
    );

    BrowserTestUtils.removeTab(newTab);
  });
});

add_task(async function load_errored_download_attribute() {
  // FIXME: In the future, we may want to have this case not load an error page
  // and instead report an error in the download UI in the future, as it's a bit
  // surprising that an `<a download>` link can cause an error page to load in a
  // new tab.
  await BrowserTestUtils.withNewTab(TEST_PATH + "blank.html", async browser => {
    info("blank loaded, adding link");

    let newTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      null,
      /* waitForLoad */ true,
      /* waitForAnyTab */ true,
      /* maybeErrorPage */ true
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.document.body.innerHTML = `<a href="emptyErrorPage.sjs" download>click</a>`;
      let link = content.document.querySelector("a");
      link.click();
    });

    let newTab = await newTabPromise;

    let documentURI = await SpecialPowers.spawn(
      newTab.linkedBrowser,
      [],
      () => content.document.documentURI
    );
    ok(
      documentURI.startsWith("about:neterror"),
      "has loaded neterror page in new tab"
    );

    BrowserTestUtils.removeTab(newTab);
  });
});
