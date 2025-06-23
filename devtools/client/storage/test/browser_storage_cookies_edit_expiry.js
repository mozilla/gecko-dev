/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies with the keyboard.

"use strict";

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  showAllColumns(true);
  showColumn("uniqueKey", false);

  const date = new Date();
  const futureExpires = date.setDate(date.getDate() + 2000);

  const id = getCookieId("test4", "test1.example.org", "/browser");
  const originalExpires = Date.parse(getRowValues(id).expires);

  await editCell(id, "expires", date.toGMTString(), false);
  const capExpires = Date.parse(getRowValues(id).expires);

  Assert.greater(
    futureExpires,
    originalExpires,
    "We have tried to set an expires greater than the original one"
  );
  Assert.greater(
    capExpires,
    originalExpires,
    "The final expires is greater than the original one"
  );
  Assert.greater(
    futureExpires,
    capExpires,
    "But still lower than the future value"
  );
});
