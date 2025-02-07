/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_removalMessage() {
  Assert.ok(
    !gNotificationBox.getNotificationWithValue("search-engine-removal"),
    "Message is not displayed initially."
  );

  BrowserUtils.callModulesFromCategory(
    { categoryName: "search-service-notification" },
    "search-engine-removal",
    "Engine 1",
    "Engine 2"
  );

  await TestUtils.waitForCondition(
    () => gNotificationBox.getNotificationWithValue("search-engine-removal"),
    "Waiting for message to be displayed"
  );
  let notificationBox = gNotificationBox.getNotificationWithValue(
    "search-engine-removal"
  );
  Assert.ok(notificationBox, "Message is displayed.");

  notificationBox.close();
});
