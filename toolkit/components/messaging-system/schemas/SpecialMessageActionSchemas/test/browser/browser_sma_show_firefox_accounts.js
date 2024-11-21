/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Note: "identity.fxaccounts.remote.root" is set to https://example.com in browser.ini
add_task(async function test_SHOW_FIREFOX_ACCOUNTS() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser);
    await SMATestUtils.executeAndValidateAction({
      type: "SHOW_FIREFOX_ACCOUNTS",
      data: { entrypoint: "snippets" },
    });
    Assert.ok(
      (await loaded).includes("entrypoint=snippets"),
      "should load fxa with endpoint=snippets"
    );

    // Open a URL
    loaded = BrowserTestUtils.browserLoaded(browser);
    await SMATestUtils.executeAndValidateAction({
      type: "SHOW_FIREFOX_ACCOUNTS",
      data: { entrypoint: "aboutwelcome" },
    });

    Assert.ok(
      (await loaded).includes("entrypoint=aboutwelcome"),
      "should load fxa with a custom endpoint"
    );

    // Open a URL with extra parameters
    loaded = BrowserTestUtils.browserLoaded(browser);
    await SMATestUtils.executeAndValidateAction({
      type: "SHOW_FIREFOX_ACCOUNTS",
      data: { entrypoint: "test", extraParams: { foo: "bar" } },
    });

    let url = await loaded;
    Assert.ok(
      url.includes("entrypoint=test") &&
        url.includes("foo=bar") &&
        url.includes("service=sync"),
      "should have correct url params"
    );
  });

  add_task(async function test_SHOW_FIREFOX_ACCOUNTS_where() {
    // Open FXA with a 'where' prop
    const action = {
      type: "SHOW_FIREFOX_ACCOUNTS",
      data: {
        entrypoint: "activity-stream-firstrun",
        where: "tab",
      },
    };
    const tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      url =>
        url.startsWith("https://example.com") &&
        url.includes("entrypoint=activity-stream-firstrun") &&
        url.includes("service=sync")
    );

    await SpecialMessageActions.handleAction(action, gBrowser);
    const browser = await tabPromise;
    ok(browser, "should open FXA in a new tab");
    BrowserTestUtils.removeTab(browser);
  });
});
