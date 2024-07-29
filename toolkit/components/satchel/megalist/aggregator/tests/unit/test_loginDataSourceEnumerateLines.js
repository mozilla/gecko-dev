/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LoginDataSource } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
);

do_get_profile();

add_task(async function test_loginDataSourceEnumerateLines() {
  const logins = testData.loginList();
  const numLogins = logins.length;
  await Services.logins.addLogins(logins);

  const loginDataSource = new LoginDataSource({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {},
    setLayout: () => {},
  });

  await TestUtils.waitForCondition(
    () => loginDataSource.doneReloadDataSource,
    "reloadDataSource has finished"
  );

  Assert.equal(
    numLogins * 3,
    loginDataSource.lines.length,
    "created three lines for each login in storage"
  );

  for (const searchText of ["", "EXAMPLE.COM"]) {
    const lines = Array.from(loginDataSource.enumerateLines(searchText));
    // remove header line
    lines.shift();

    // call getAllLogins to get guids
    const loginsWithGuid = await Services.logins.getAllLogins();

    const filterdLogins = loginsWithGuid.filter(login => {
      const matchFn = loginMatch(searchText);
      return matchFn(login);
    });

    LoginTestUtils.assertLoginListsEqual(
      filterdLogins,
      [...new Set(lines.map(line => line.record))],
      "correctly filtered lines",
      (a, b) => a.guid === b.guid
    );

    for (let i = 0; i < lines.length; i += 3) {
      const guid1 = lines[i].record.guid;
      const guid2 = lines[i + 1].record.guid;
      const guid3 = lines[i + 2].record.guid;
      const sameGuidAmongLines =
        guid1 === guid2 && guid2 === guid3 && guid1 === guid3;
      Assert.ok(
        sameGuidAmongLines,
        "three consecutive lines are from the same login"
      );
    }
  }
});
