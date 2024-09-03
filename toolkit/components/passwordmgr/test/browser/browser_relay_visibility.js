const TEST_URL_PATH = `https://example.org${DIRECTORY_PATH}form_basic_signup.html`;
let gRelayACOptionsTitles;

async function getRelayACItem(browser) {
  const popup = document.getElementById("PopupAutoComplete");
  await openACPopup(popup, browser, "#form-basic-username");
  const popupItemAttributeValue = document
    .querySelector("richlistitem")
    .getAttribute("ac-value");
  const relayItemInPopup = gRelayACOptionsTitles.some(
    title => title.value === popupItemAttributeValue
  );
  return relayItemInPopup;
}

add_setup(async function () {
  gRelayACOptionsTitles = await new Localization([
    "browser/firefoxRelay.ftl",
    "toolkit/branding/brandings.ftl",
  ]).formatMessages([
    "firefox-relay-opt-in-title-1",
    "firefox-relay-use-mask-title",
  ]);
});

add_task(async function test_popup_option_default_not_visible() {
  await BrowserTestUtils.withNewTab(TEST_URL_PATH, async browser => {
    const relayItemInPopup = await getRelayACItem(browser);
    Assert.ok(!relayItemInPopup);
  });
});

add_task(async function test_popup_option_visible_when_enabled_for_all() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.firefoxRelay.showToAllBrowsers", true]],
  });
  await BrowserTestUtils.withNewTab(TEST_URL_PATH, async browser => {
    const relayItemInPopup = await getRelayACItem(browser);
    Assert.ok(relayItemInPopup);
  });
});
