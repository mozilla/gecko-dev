/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  registerCleanupFunction(async () => {
    UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.lastDismissedTime");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.dismissedCount");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.firstImpressionTime");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.impressionCount");
    UrlbarPrefs.clear(
      "quicksuggest.contextualOptIn.firstReshowAfterPeriodDays"
    );
    UrlbarPrefs.clear(
      "quicksuggest.contextualOptIn.secondReshowAfterPeriodDays"
    );
    UrlbarPrefs.clear(
      "quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays"
    );
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.impressionLimit");
    UrlbarPrefs.clear("quicksuggest.contextualOptIn.impressionDaysLimit");
  });
});

add_task(async function accept() {
  info("Setup");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("quicksuggest.contextualOptIn", true);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.dismissedCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.lastDismissedTime", 0);

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

  info(
    "Check whether the contextual opt-in result was removed from last query"
  );
  let { queryContext } = gURLBar.controller._lastQueryContextWrapper;
  Assert.ok(
    !queryContext.results.some(
      r => r.providerName == "UrlbarProviderQuickSuggestContextualOptIn"
    )
  );
});

add_task(async function dismiss() {
  info("Setup");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("quicksuggest.contextualOptIn", true);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.lastDismissedTime", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.dismissedCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.firstReshowAfterPeriodDays", 2);
  UrlbarPrefs.set(
    "quicksuggest.contextualOptIn.secondReshowAfterPeriodDays",
    3
  );
  UrlbarPrefs.set("quicksuggest.contextualOptIn.impressionCount", 0);
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.impressionLimit");
  UrlbarPrefs.set("quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays", 4);

  info("First dismissal");
  await assertContextualOptinVisibility({ visible: true });
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    1
  );
  await assertContextualOptinVisibility({ visible: false });

  info(
    "Move lastDismissedTime date 1 day earlier than current lastDismissedTime"
  );
  let lastDismissedTime = UrlbarPrefs.get(
    "quicksuggest.contextualOptIn.lastDismissedTime"
  );
  moveLastDismissedTimeEalier(lastDismissedTime, 1);
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissedTime date 2 days earlier");
  moveLastDismissedTimeEalier(lastDismissedTime, 2);
  await assertContextualOptinVisibility({ visible: true });

  info("Second dismissal");
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    2
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissedTime date 2 days earlier");
  lastDismissedTime = UrlbarPrefs.get(
    "quicksuggest.contextualOptIn.lastDismissedTime"
  );
  moveLastDismissedTimeEalier(lastDismissedTime, 2);
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissedTime date 3 days earlier");
  moveLastDismissedTimeEalier(lastDismissedTime, 3);
  await assertContextualOptinVisibility({ visible: true });

  info("Third dismissal");
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    3
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissedTime date 3 days earlier");
  lastDismissedTime = UrlbarPrefs.get(
    "quicksuggest.contextualOptIn.lastDismissedTime"
  );
  moveLastDismissedTimeEalier(lastDismissedTime, 3);
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissedTime date 4 days earlier");
  moveLastDismissedTimeEalier(lastDismissedTime, 4);
  await assertContextualOptinVisibility({ visible: true });

  info("Fourth dismissal");
  await doDismiss();
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    4
  );
  await assertContextualOptinVisibility({ visible: false });

  info("Move lastDismissedTime date 360 days earlier");
  lastDismissedTime = UrlbarPrefs.get(
    "quicksuggest.contextualOptIn.lastDismissedTime"
  );
  moveLastDismissedTimeEalier(lastDismissedTime, 360);
  await assertContextualOptinVisibility({ visible: false });
});

add_task(async function dismiss_by_impression_count() {
  info("Setup");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("quicksuggest.contextualOptIn", true);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.dismissedCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.lastDismissedTime", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.impressionCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.impressionLimit", 3);

  info(
    "Contextual opt-in result should show until reaching to impressionLimit"
  );
  for (let i = 0; i < 2; i++) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(
      result.providerName,
      "UrlbarProviderQuickSuggestContextualOptIn"
    );
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());

    Assert.equal(
      UrlbarPrefs.get("quicksuggest.contextualOptIn.impressionCount"),
      i + 1
    );
    Assert.equal(
      UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
      0
    );
  }

  info(
    "Contextual opt-in result should be dismissed when reaching to impressionLimit"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    1
  );
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.impressionCount"),
    0
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.notEqual(
    result.providerName,
    "UrlbarProviderQuickSuggestContextualOptIn"
  );
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function dismiss_by_past_days_after_first_impression() {
  info("Setup");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("quicksuggest.contextualOptIn", true);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.dismissedCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.lastDismissedTime", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.impressionCount", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.firstImpressionTime", 0);
  UrlbarPrefs.set("quicksuggest.contextualOptIn.impressionDaysLimit", 5);

  info("Show the contextual opt-in message, but not interacte to it");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  let result = (await UrlbarTestUtils.getDetailsOfResultAt(window, 0)).result;
  Assert.equal(
    result.providerName,
    "UrlbarProviderQuickSuggestContextualOptIn"
  );
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  Assert.ok(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.firstImpressionTime")
  );

  info("Simulate 4 days later");
  moveFirstImpressionTimeEalier(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.firstImpressionTime"),
    4
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    0,
    "Still not dismissed"
  );

  info("Simulate 1 more day later (total 5 days)");
  moveFirstImpressionTimeEalier(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.firstImpressionTime"),
    1
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.dismissedCount"),
    1,
    "Dismissed"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  result = (await UrlbarTestUtils.getDetailsOfResultAt(window, 0)).result;
  Assert.notEqual(
    result.providerName,
    "UrlbarProviderQuickSuggestContextualOptIn"
  );
  await UrlbarTestUtils.promisePopupClose(window);
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

function moveLastDismissedTimeEalier(lastDismissedTime, days) {
  let date = new Date(lastDismissedTime * 1000);
  date.setDate(date.getDate() - days);
  UrlbarPrefs.set(
    "quicksuggest.contextualOptIn.lastDismissedTime",
    date.getTime() / 1000
  );
}

function moveFirstImpressionTimeEalier(firstImpressionTime, days) {
  let date = new Date(firstImpressionTime * 1000);
  date.setDate(date.getDate() - days);
  UrlbarPrefs.set(
    "quicksuggest.contextualOptIn.firstImpressionTime",
    date.getTime() / 1000
  );
}

add_task(async function nimbus() {
  UrlbarPrefs.clear("quicksuggest.contextualOptIn");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.firstReshowAfterPeriodDays");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.secondReshowAfterPeriodDays");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.thirdReshowAfterPeriodDays");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.impressionLimit");
  UrlbarPrefs.clear("quicksuggest.contextualOptIn.impressionDaysLimit");

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
      ["browser.urlbar.quicksuggest.contextualOptIn.impressionLimit", 20],
      ["browser.urlbar.quicksuggest.contextualOptIn.impressionDaysLimit", 5],
    ],
  });

  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    quickSuggestContextualOptInEnabled: true,
    quickSuggestContextualOptInFirstReshowAfterPeriodDays: 100,
    quickSuggestContextualOptInSecondReshowAfterPeriodDays: 200,
    quickSuggestContextualOptInThirdReshowAfterPeriodDays: 300,
    quickSuggestContextualOptInImpressionLimit: 400,
    quickSuggestContextualOptInImpressionDaysLimit: 500,
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
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.impressionLimit"),
    400
  );
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.contextualOptIn.impressionDaysLimit"),
    500
  );

  await cleanUpNimbus();
  await SpecialPowers.popPrefEnv();
});
