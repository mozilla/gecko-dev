/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests view updates in relation to hidden-exposure results.

"use strict";

// Tests the case where a hidden-exposure result replaces another result in a
// row in the view.
add_task(async function rowCanUpdateToResult() {
  // Create a provider that returns two non-hidden results.
  let provider = new UrlbarTestUtils.TestProvider({ priority: Infinity });
  UrlbarProvidersManager.registerProvider(provider);
  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(provider);
  });

  for (let i = 0; i < 2; i++) {
    provider.results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          url: "https://example.com/" + i,
        }
      )
    );
  }

  // Do a search to populate the view with the provider's results, and leave the
  // view open.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test1",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "The view should have the two non-hidden provider results"
  );
  for (let i = 0; i < 2; i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(
      details.url,
      "https://example.com/" + i,
      "The non-hidden result should have the expected URL at index " + i
    );
  }

  // Now make the provider return only a hidden-exposure result. Important: The
  // hidden-exposure result needs to pass the view's "row can update to result?"
  // check so that it can replace the non-hidden result in the first row. So
  // make sure the two results are the exact same type.
  provider.results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          url: "https://example.com/hidden-exposure",
        }
      ),
      {
        exposureTelemetry: UrlbarUtils.EXPOSURE_TELEMETRY.HIDDEN,
      }
    ),
  ];

  // Do another search without closing the view first. Since the only result is
  // the hidden result, there should no longer be any rows in the view.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test2",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    0,
    "The view should be empty"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.handleRevert();

  UrlbarProvidersManager.unregisterProvider(provider);
});
