/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kWidgetId = "pbm-only-test-widget";

function assertWidgetExists(aWindow, aExists) {
  if (aExists) {
    ok(
      aWindow.document.getElementById(kWidgetId),
      "Should have found test widget in the window"
    );
  } else {
    is(
      aWindow.document.getElementById(kWidgetId),
      null,
      "Should not have found test widget in the window"
    );
  }
}

// A widget that is created with hideInNonPrivateBrowsing undefined should
// have that value default to false.
add_task(function () {
  let wrapper = CustomizableUI.createWidget({
    id: kWidgetId,
  });
  ok(
    !wrapper.hideInNonPrivateBrowsing,
    "hideInNonPrivateBrowsing should have defaulted to false."
  );
  CustomizableUI.destroyWidget(kWidgetId);
});

// Add a widget via the API with hideInNonPrivateBrowsing set to true
// and ensure it does not appear in pre-existing or newly created
// non-private windows.
add_task(async function () {
  let plain1 = await openAndLoadWindow();
  let private1 = await openAndLoadWindow({ private: true });
  CustomizableUI.createWidget({
    id: kWidgetId,
    removable: true,
    hideInNonPrivateBrowsing: true,
  });
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);
  assertWidgetExists(plain1, false);
  assertWidgetExists(private1, true);

  // Now open up some new windows. The widget should not exist in the new
  // plain window, but exist in the new private window.
  let plain2 = await openAndLoadWindow();
  let private2 = await openAndLoadWindow({ private: true });
  assertWidgetExists(plain2, false);
  assertWidgetExists(private2, true);

  // Try moving the widget around and make sure it doesn't get added
  // to the non-private windows. We'll start by appending it to the tabstrip.
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_TABSTRIP);
  assertWidgetExists(plain1, false);
  assertWidgetExists(plain2, false);
  assertWidgetExists(private1, true);
  assertWidgetExists(private2, true);

  // And then move it to the beginning of the tabstrip.
  CustomizableUI.moveWidgetWithinArea(kWidgetId, 0);
  assertWidgetExists(plain1, false);
  assertWidgetExists(plain2, false);
  assertWidgetExists(private1, true);
  assertWidgetExists(private2, true);

  CustomizableUI.removeWidgetFromArea(kWidgetId);
  assertWidgetExists(plain1, false);
  assertWidgetExists(plain2, false);
  assertWidgetExists(private1, false);
  assertWidgetExists(private2, false);

  await Promise.all(
    [plain1, plain2, private1, private2].map(promiseWindowClosed)
  );

  CustomizableUI.destroyWidget(kWidgetId);
});

// Add a widget via the API with hideInNonPrivateBrowsing set to false,
// and ensure that it appears in pre-existing or newly created
// private browsing windows.
add_task(async function () {
  let plain1 = await openAndLoadWindow();
  let private1 = await openAndLoadWindow({ private: true });

  CustomizableUI.createWidget({
    id: kWidgetId,
    removable: true,
    hideInNonPrivateBrowsing: false,
  });
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);
  assertWidgetExists(plain1, true);
  assertWidgetExists(private1, true);

  // Now open up some new windows. The widget should exist in the new
  // plain window, but not the new private window.
  let plain2 = await openAndLoadWindow();
  let private2 = await openAndLoadWindow({ private: true });

  assertWidgetExists(plain2, true);
  assertWidgetExists(private2, true);

  // Try moving the widget around and make sure it doesn't get added
  // to the private windows. We'll start by appending it to the tabstrip.
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_TABSTRIP);
  assertWidgetExists(plain1, true);
  assertWidgetExists(plain2, true);
  assertWidgetExists(private1, true);
  assertWidgetExists(private2, true);

  // And then move it to the beginning of the tabstrip.
  CustomizableUI.moveWidgetWithinArea(kWidgetId, 0);
  assertWidgetExists(plain1, true);
  assertWidgetExists(plain2, true);
  assertWidgetExists(private1, true);
  assertWidgetExists(private2, true);

  CustomizableUI.removeWidgetFromArea(kWidgetId);
  assertWidgetExists(plain1, false);
  assertWidgetExists(plain2, false);
  assertWidgetExists(private1, false);
  assertWidgetExists(private2, false);

  await Promise.all(
    [plain1, plain2, private1, private2].map(promiseWindowClosed)
  );

  CustomizableUI.destroyWidget(kWidgetId);
});

// Add a widget via the API with hideInNonPrivateBrowsing set to true
// and ensure it does not appear in the list of unused widgets in private
// windows.
add_task(async function testPrivateBrowsingCustomizeModeWidget() {
  CustomizableUI.createWidget({
    id: kWidgetId,
    hideInNonPrivateBrowsing: true,
  });

  let normalWidgetArray = CustomizableUI.getUnusedWidgets(gNavToolbox.palette);
  normalWidgetArray = normalWidgetArray.map(w => w.id);
  is(
    normalWidgetArray.indexOf(kWidgetId),
    -1,
    "Widget should not appear as unused in non-private window"
  );

  let privateWindow = await openAndLoadWindow({ private: true });
  let privateWidgetArray = CustomizableUI.getUnusedWidgets(
    privateWindow.gNavToolbox.palette
  );
  privateWidgetArray = privateWidgetArray.map(w => w.id);
  Assert.greater(
    privateWidgetArray.indexOf(kWidgetId),
    -1,
    "Widget should appear as unused in private window"
  );
  await promiseWindowClosed(privateWindow);

  CustomizableUI.destroyWidget(kWidgetId);
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
