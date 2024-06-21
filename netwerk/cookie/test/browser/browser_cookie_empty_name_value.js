"use strict";

const EMPTYNAMEVALUE_DOMAIN = "https://example.com/";
const EMPTYNAMEVALUE_PATH = "browser/netwerk/cookie/test/browser/";
const EMPTYNAMEVALUE_TOP_PAGE =
  EMPTYNAMEVALUE_DOMAIN + EMPTYNAMEVALUE_PATH + "cookie_empty_name_value.sjs";

add_setup(async function () {
  Services.cookies.removeAll();

  registerCleanupFunction(async function () {
    Services.cookies.removeAll();
  });
});

add_task(async _ => {
  const expected = [];

  const consoleListener = {
    observe(what) {
      if (!(what instanceof Ci.nsIConsoleMessage)) {
        return;
      }

      info("Console Listener: " + what);
      for (let i = expected.length - 1; i >= 0; --i) {
        const e = expected[i];

        if (what.message.includes(e.match)) {
          ok(true, "Message received: " + e.match);
          expected.splice(i, 1);
          e.resolve();
        }
      }
    },
  };

  Services.console.registerListener(consoleListener);

  registerCleanupFunction(() =>
    Services.console.unregisterListener(consoleListener)
  );

  const netPromises = [
    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie with an empty name and an empty value has been rejected.",
      });
    }),
  ];

  // Let's open our tab.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    EMPTYNAMEVALUE_TOP_PAGE
  );
  const browser = gBrowser.getBrowserForTab(tab);

  // Let's wait for the first set of console events.
  await Promise.all(netPromises);

  SpecialPowers.spawn(browser, [], () => {
    Assert.strictEqual(content.document.cookie, "", "No cookies set");
  });

  // the DOM list of events.
  const domPromises = [
    new Promise(resolve => {
      expected.push({
        resolve,
        match:
          "Cookie with an empty name and an empty value has been rejected.",
      });
    }),
  ];

  // Let's use document.cookie
  SpecialPowers.spawn(browser, [], () => {
    content.document.cookie = " ; path=/; secure";
  });

  // Let's wait for the dom events.
  await Promise.all(domPromises);

  SpecialPowers.spawn(browser, [], () => {
    Assert.strictEqual(content.document.cookie, "", "No cookies set");
  });

  // Let's close the tab.
  BrowserTestUtils.removeTab(tab);
});
