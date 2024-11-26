/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LoginDataSource } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
);

do_get_profile();

add_task(async function test_loginDataSourceUsernamePrototype() {
  const login = await LoginTestUtils.addLogin({
    origin: "https://example.com",
    username: "username",
    password: "pass",
  });

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
    loginDataSource.lines.length,
    3,
    "reloadDataSource has finished"
  );

  let usernameLine = loginDataSource.lines[1];

  Assert.equal(
    usernameLine.value,
    login.username,
    "username of line matches username of login record"
  );
});
