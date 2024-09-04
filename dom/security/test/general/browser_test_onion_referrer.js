// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const ONION_BASE = "http://example.onion/";

const ONION_PATH =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    ONION_BASE
  ) + "file_empty.html";

const ECHOER_PATH =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  ) + "file_referrer_echoer.sjs";

function getReferrer(browser) {
  return SpecialPowers.spawn(browser, [ECHOER_PATH], async url =>
    content.fetch(url).then(response => response.text())
  );
}

async function runTest(hideOnionSource) {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, ONION_PATH);

  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.hideOnionSource", hideOnionSource]],
  });

  const referer = await getReferrer(tab.linkedBrowser);
  const expectedReferrer = hideOnionSource ? "" : ONION_BASE;
  const expectedReferrerMessage = hideOnionSource ? "hidden" : "visible";
  is(
    referer,
    expectedReferrer,
    `The referrer should be ${expectedReferrerMessage}. Got: ${referer}`
  );

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_onion_referrer_visible() {
  await runTest(false);
});

add_task(async function test_onion_referrer_hidden() {
  await runTest(true);
});
