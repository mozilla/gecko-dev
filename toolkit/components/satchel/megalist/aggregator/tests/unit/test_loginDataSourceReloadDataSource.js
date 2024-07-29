/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LoginDataSource } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
);

do_get_profile();

add_task(async function test_loginDataSourceReloadDataSource() {
  const logins = testData.loginList();
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
    logins.length * 3,
    loginDataSource.lines.length,
    "created three lines for each login in storage."
  );

  const removedLogin = logins.pop();
  await LoginTestUtils.removeLogin(removedLogin);

  await TestUtils.waitForCondition(
    () => loginDataSource.doneReloadDataSource,
    "reloadDataSource has finished"
  );

  Assert.equal(
    logins.length * 3,
    loginDataSource.lines.length,
    "removed three lines when a login was removed form storage."
  );
  LoginTestUtils.clearData();
});

add_task(
  async function test_loginDataSourceEnumerateLinesRememberSignonsPrefOff() {
    const logins = testData.loginList();

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

    Services.prefs.setBoolPref("signon.rememberSignons", false);

    await TestUtils.waitForCondition(
      () => loginDataSource.doneReloadDataSource,
      "reloadDataSource has finished"
    );

    Assert.ok(
      !loginDataSource.lines.length,
      "if rememberSignons pref is off, no logins should be loaded"
    );
  }
);
