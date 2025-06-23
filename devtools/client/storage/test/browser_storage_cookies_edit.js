/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies.

"use strict";

add_task(async function () {
  const { toolbox, storage } = await openTabAndSetupStorage(
    MAIN_DOMAIN + "storage-cookies.html"
  );
  const doc = storage.panelWindow.document;
  showAllColumns(true);

  let id = getCookieId("test3", ".test1.example.org", "/browser");
  await editCell(id, "name", "newTest3");

  id = getCookieId("newTest3", ".test1.example.org", "/browser");
  await editCell(id, "host", "test1.example.org");

  id = getCookieId("newTest3", "test1.example.org", "/browser");
  await editCell(id, "path", "/");

  const date = new Date();
  date.setDate(date.getDate() + 8);

  id = getCookieId("newTest3", "test1.example.org", "/");
  await editCell(id, "expires", date.toGMTString());
  await editCell(id, "value", "newValue3");
  await editCell(id, "isSecure", "true");
  await editCell(id, "isHttpOnly", "true");

  info(
    "Check that setting values that make the cookie invalid displays a warning"
  );

  // Sanity check
  is(
    toolbox.doc.querySelector(".notificationbox"),
    null,
    "No warning is displayed after editing valid cookies"
  );
  isnot(
    getCellLabelElement(doc, "newTest3"),
    null,
    "The expected name cell exists"
  );

  id = getCookieId("newTest3", "test1.example.org", "/");
  // Adding a space prefix does make the cookie invalid
  const invalidCookieName = " thisIsInvalid";
  await editCell(id, "name", invalidCookieName, /* validate */ false);

  const notificationBox = await waitFor(() =>
    toolbox.doc.querySelector(".notificationbox")
  );

  const notificationEl = notificationBox.querySelector(".notification");
  is(
    notificationEl.getAttribute("data-key"),
    "storage-cookie-edit-error",
    "Notification has expected key"
  );
  is(
    notificationEl.getAttribute("data-type"),
    "warning",
    "Notification is a warning"
  );

  const message = notificationEl.textContent;
  ok(
    notificationEl.textContent.startsWith("Cookie could not be updated"),
    `The warning message is rendered as expected (${message})`
  );

  is(
    getCellLabelElement(doc, invalidCookieName),
    null,
    "The invalid cookie isn't visible anymore"
  );

  isnot(
    getRowItem(getCookieId("newTest3", "test1.example.org", "/")),
    null,
    "The cookie row for the original cookie is displayed"
  );
  isnot(
    getCellLabelElement(doc, "newTest3"),
    null,
    "The original cookie name is visible"
  );
});

function getCellLabelElement(doc, labelValue) {
  return doc.querySelector(
    `#name.table-widget-column .table-widget-cell[value="${labelValue}"]`
  );
}
