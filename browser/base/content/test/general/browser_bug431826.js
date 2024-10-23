add_task(async function () {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "https://nocert.example.com/"
  );
  await promise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    // Confirm that we are displaying the contributed error page, not the default
    let uri = content.document.documentURI;
    Assert.ok(
      uri.startsWith("about:certerror"),
      "Broken page should go to about:certerror, not about:neterror"
    );
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    let div = content.document.getElementById("badCertAdvancedPanel");
    // Confirm that the expert section is collapsed
    Assert.ok(div, "Advanced content div should exist");
    Assert.equal(
      div.ownerGlobal.getComputedStyle(div).display,
      "none",
      "Advanced content should not be visible by default"
    );
  });

  // Tweak the expert mode pref
  Services.prefs.setBoolPref("browser.xul.error_pages.expert_bad_cert", true);

  promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  gBrowser.reload();
  await promise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    let div = content.document.getElementById("badCertAdvancedPanel");
    Assert.ok(div, "Advanced content div should exist");
    Assert.equal(
      div.ownerGlobal.getComputedStyle(div).display,
      "block",
      "Advanced content should be visible by default"
    );
  });

  // Clean up
  gBrowser.removeCurrentTab();
  if (
    Services.prefs.prefHasUserValue("browser.xul.error_pages.expert_bad_cert")
  ) {
    Services.prefs.clearUserPref("browser.xul.error_pages.expert_bad_cert");
  }
});

add_task(async function testWithFeltPrivacyEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.certerrors.felt-privacy-v1", true]],
  });

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "https://nocert.example.com/"
  );
  await promise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    // Confirm that we are displaying the contributed error page, not the default
    let uri = content.document.documentURI;
    Assert.ok(
      uri.startsWith("about:certerror"),
      "Broken page should go to about:certerror, not about:neterror"
    );
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    let netErrorCard =
      content.document.querySelector("net-error-card").wrappedJSObject;
    await netErrorCard.updateComplete;
    Assert.ok(
      netErrorCard.advancedShowing,
      "Advanced showing attribute should be true"
    );
    await ContentTaskUtils.waitForCondition(
      () =>
        netErrorCard.advancedContainer &&
        ContentTaskUtils.isVisible(netErrorCard.advancedContainer),
      "Wait for advanced content to exist"
    );

    Assert.ok(
      ContentTaskUtils.isVisible(netErrorCard.advancedContainer),
      "Advanced content div should be visible"
    );
  });

  // Clean up
  gBrowser.removeCurrentTab();

  await SpecialPowers.popPrefEnv();
});
