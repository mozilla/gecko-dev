/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LoginDataSource } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
);

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

do_get_profile();

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

ChromeUtils.defineESModuleGetters(this, {
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
});

async function add_breach() {
  const breach = {
    AddedDate: "2018-12-20T23:56:26Z",
    BreachDate: "2018-12-16",
    Domain: "example.com",
    Name: "example",
    PwnCount: 1643100,
    DataClasses: ["Email addresses", "Usernames", "Passwords", "IP addresses"],
    _status: "synced",
    id: "047940fe-d2fd-4314-b636-b4a952ee0044",
    last_modified: "1541615610052",
    schema: "1541615609018",
  };
  async function emitSync() {
    await RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).emit(
      "sync",
      { data: { current: [breach] } }
    );
  }

  gBrowserGlue.observe(null, "browser-glue-test", "add-breaches-sync-handler");
  const db = RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).db;
  await db.importChanges({}, Date.now(), [breach]);
  await emitSync();
}

add_task(async function test_loginDataSourceOriginPrototype() {
  await add_breach();

  const newLogin = testData.formLogin({
    origin: "https://example.com",
    formActionOrigin: "https://example.com",
    username: "username",
    password: "pass",
    timePasswordChanged: new Date("2015-12-15").getTime(),
  });

  await Services.logins.addLoginAsync(newLogin);

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
    "added three lines for a login"
  );

  const originLine = loginDataSource.lines[0];

  Assert.equal(
    originLine.value,
    newLogin.displayOrigin,
    "display origin of line matches display origin of login record"
  );
  Assert.equal(
    originLine.href,
    newLogin.origin,
    "href of line matches origin of login record"
  );
  Assert.ok(originLine.breached, "line is marked as breached");

  originLine.executeDismissBreach();
  Assert.ok(
    !originLine.breached,
    "line is not marked as breached after dismissal"
  );
});
