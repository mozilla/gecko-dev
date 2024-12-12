/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  info("Starting first party tracker test");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.enabled", true],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });
  await UrlClassifierTestUtils.addTestTrackers();

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://test1.example.com/browser/toolkit/components/antitracking/test/browser/empty.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Create a cross-site iframe.");
  await SpecialPowers.spawn(browser, [], async () => {
    const iframe = content.document.createElement("iframe");
    iframe.src =
      "https://example.org/browser/toolkit/components/antitracking/test/browser/empty.html";
    let loaded = new Promise(resolve => {
      iframe.addEventListener("load", resolve, { once: true });
    });
    content.document.body.appendChild(iframe);
    await loaded;
  });

  info("Create make a same-site-to-top-level request inside that iframe.");
  let result = await SpecialPowers.spawn(browser, [], async function () {
    const iframe = content.document.querySelector("iframe");
    const result = await SpecialPowers.spawn(iframe, [], async () => {
      info("Image loading ...");
      let loading = new content.Promise(resolve => {
        let image = new content.Image();
        image.src =
          "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/raptor.jpg?" +
          Math.random();
        image.onload = _ => resolve(true);
        image.onerror = _ => resolve(false);
      });
      return loading;
    });
    return result;
  });
  ok(result, "Same-site-to-top-level should never be a tracker.");

  info("Create make a cross-site-to-top-level request inside that iframe.");
  result = await SpecialPowers.spawn(browser, [], async function () {
    const iframe = content.document.querySelector("iframe");
    const result = await SpecialPowers.spawn(iframe, [], async () => {
      info("Image loading ...");
      let loading = new content.Promise(resolve => {
        let image = new content.Image();
        image.src =
          "https://itisatracker.org/browser/toolkit/components/antitracking/test/browser/raptor.jpg?" +
          Math.random();
        image.onload = _ => resolve(true);
        image.onerror = _ => resolve(false);
      });
      return loading;
    });
    return result;
  });
  ok(!result, "Sanity check on a different tracker.");

  info("Checking the content blocking log");
  let log = JSON.parse(browser.getContentBlockingLog());
  is(
    log["https://tracking.example.com"],
    undefined,
    "Must not have an entry for a same-site tracker."
  );
  is(
    log["https://itisatracker.org"].length,
    1,
    "Entry for blocking a tracker exists."
  );
  is(
    log["https://itisatracker.org"][0][0],
    Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT,
    "Entry for blocking a tracker has the right flag."
  );
  is(
    log["https://itisatracker.org"][0][1],
    true,
    "Entry for blocking a tracker has the right 'blocked' boolean."
  );
  is(
    log["https://itisatracker.org"][0][2],
    1,
    "Entry for blocking a tracker has the right count."
  );

  UrlClassifierTestUtils.cleanupTestTrackers();
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.flushPrefEnv();
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
});
