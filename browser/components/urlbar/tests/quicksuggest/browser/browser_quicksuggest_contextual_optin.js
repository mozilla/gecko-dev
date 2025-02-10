/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  registerCleanupFunction(async () => {
    UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.lastDismissed");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.dismissedCount");
    UrlbarPrefs.clear(
      "quicksuggest.contextualOptIn.firstReshowAfterPeriodDays"
    );
    UrlbarPrefs.clear(
      "quicksuggest.contextualOptIn.secondReshowAfterPeriodDays"
    );
    UrlbarPrefs.clear(
      "quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays"
    );
  });
});

add_task(async function accept() {
  info("Setup");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("quicksuggest.contextualOptIn", true);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.dismissedCount", 0);

  info("Open urlbar results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Check the contextual opt-in result");
  let { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    0
  );
  Assert.equal(
    result.providerName,
    "UrlbarProviderQuickSuggestContextualOptIn"
  );

  info("Accept the contextual opt-in");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(
      element.row.querySelector("[name=allow]"),
      {},
      window
    );
  });
  Assert.ok(UrlbarPrefs.get("quicksuggest.dataCollection.enabled"));
});

add_task(async function dismiss() {
  info("Setup");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("quicksuggest.contextualOptIn", true);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.lastDismissed", "");
  UrlbarPrefs.set("quicksuggest.contextualOptIn.dismissedCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.firstReshowAfterPeriodDays", 2);
  UrlbarPrefs.set(
    "quicksuggest.contextualOptIn.secondReshowAfterPeriodDays",
    3
  );
  UrlbarPrefs.set("quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays", 4);

  info("First dismissal");
  await assertContextualOptinVisibility({ visible: true });
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    1
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 1 day earlier than current lastDismissed");
  let lastDismissed = UrlbarPrefs.get(
    "quicksuggest.contextualOptIn.lastDismissed"
  );
  moveLastDismissedEalier(lastDismissed, 1);
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 2 days earlier");
  moveLastDismissedEalier(lastDismissed, 2);
  await assertContextualOptinVisibility({ visible: true });

  info("Second dismissal");
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    2
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 2 days earlier");
  lastDismissed = UrlbarPrefs.get("quicksuggest.contextualOptIn.lastDismissed");
  moveLastDismissedEalier(lastDismissed, 2);
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 3 days earlier");
  moveLastDismissedEalier(lastDismissed, 3);
  await assertContextualOptinVisibility({ visible: true });

  info("Third dismissal");
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    3
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 3 days earlier");
  lastDismissed = UrlbarPrefs.get("quicksuggest.contextualOptIn.lastDismissed");
  moveLastDismissedEalier(lastDismissed, 3);
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 4 days earlier");
  moveLastDismissedEalier(lastDismissed, 4);
  await assertContextualOptinVisibility({ visible: true });

  info("Fourth dismissal");
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    4
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissed date 360 days earlier");
  lastDismissed = UrlbarPrefs.get("quicksuggest.contextualOptIn.lastDismissed");
  moveLastDismissedEalier(lastDismissed, 360);
  await assertContextualOptinVisibility({ visible: false });
});

async function doDismiss() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  let { element } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  info("Dismiss the contextual opt-in");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(
      element.row.querySelector("[name=dismiss]"),
      {},
      window
    );
  });
}

async function assertContextualOptinVisibility({ visible }) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  if (visible) {
    Assert.equal(
      result.providerName,
      "UrlbarProviderQuickSuggestContextualOptIn",
      "Contextual opt-in result shoud be shown"
    );
  } else {
    Assert.notEqual(
      result.providerName,
      "UrlbarProviderQuickSuggestContextualOptIn",
      "Contextual opt-in result shoud not be shown"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
}

function moveLastDismissedEalier(lastDismissed, days) {
  let date = new Date(lastDismissed);
  date.setDate(date.getDate() - days);
  UrlbarPrefs.set(
    "quicksuggest.contextualOptIn.lastDismissed",
    date.toISOString()
  );
}

add_task(async function nimbus() {
  UrlbarPrefs.clear("quicksuggest.contextualOptIn");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.firstReshowAfterPeriodDays");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.secondReshowAfterPeriodDays");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.contextualOptIn", false],
      [
        "browser.urlbar.quicksuggest.contextualOptIn.firstReshowAfterPeriodDays",
        7,
      ],
      [
        "browser.urlbar.quicksuggest.contextualOptIn.secondReshowAfterPeriodDays",
        14,
      ],
      [
        "browser.urlbar.quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays",
        60,
      ],
    ],
  });

  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    quickSuggestContextualOptInEnabled: true,
    quickSuggestContextualOptInFirstReshowAfterPeriodDays: 100,
    quickSuggestContextualOptInSecondReshowAfterPeriodDays: 200,
    quickSuggestContextualOptInThirdReshowAfterPeriodDays: 300,
  });

  Assert.equal(UrlbarPrefs.get("quicksuggest.contextualOptIn"), true);
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.firstReshowAfterPeriodDays"),
    100
  );
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.secondReshowAfterPeriodDays"),
    200
  );
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays"),
    300
  );

  await cleanUpNimbus();
  await SpecialPowers.popPrefEnv();
});
