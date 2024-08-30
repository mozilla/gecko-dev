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

const LOGINS = [
  {
    origin: "https://b.com",
    username: "aUsername",
    password: "pass",
  },
  {
    origin: "https://a.com",
    username: "bUsername",
    password: "pass",
  },
  {
    origin: "https://b.com",
    username: "bUsername",
    password: "pass",
  },
  {
    origin: "https://a.com",
    username: "aUsername",
    password: "pass",
  },
];

function getDisplayOriginAndUsername(lines) {
  const displayOriginAndUsername = [];
  for (let i = 0; i < lines.length; i += 3) {
    displayOriginAndUsername.push({
      displayOrigin: lines[i].record.displayOrigin,
      username: lines[i].record.username,
    });
  }
  return displayOriginAndUsername;
}

async function addBreach() {
  const breach = {
    AddedDate: "2018-12-20T23:56:26Z",
    BreachDate: "2018-12-16",
    Domain: "breached.com",
    Name: "breached",
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

add_task(async function test_loginDataSourceSortByName() {
  for (const login of LOGINS) {
    await LoginTestUtils.addLogin(login);
  }
  const loginDataSource = new LoginDataSource({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {},
    setLayout: () => {},
  });

  await TestUtils.waitForCondition(
    () => loginDataSource.doneReloadDataSource,
    "reloadDataSource has finished"
  );

  const displayOriginAndUsername = getDisplayOriginAndUsername(
    loginDataSource.lines
  );

  const expectedLinesOrder = [
    {
      displayOrigin: "a.com",
      username: "aUsername",
    },
    {
      displayOrigin: "a.com",
      username: "bUsername",
    },
    {
      displayOrigin: "b.com",
      username: "aUsername",
    },
    {
      displayOrigin: "b.com",
      username: "bUsername",
    },
  ];

  // Logins should be sorted first by displayOrigin (ignoring domain), then username, and then guid.
  Assert.deepEqual(
    expectedLinesOrder,
    displayOriginAndUsername,
    "lines were added in the correct order"
  );

  LoginTestUtils.clearData();
});

add_task(async function test_loginDataSourceSortByAlerts() {
  for (const login of LOGINS) {
    await LoginTestUtils.addLogin(login);
  }

  await addBreach();

  const breachedOrVulnerableLogins = [
    testData.formLogin({
      origin: "https://breached.com",
      formActionOrigin: "https://breached.com",
      username: "breached",
      password: "breachedpass",
      timePasswordChanged: new Date("2015-12-15").getTime(),
    }),
    testData.formLogin({
      origin: "https://vulnerablelogin.com",
      formActionOrigin: "https://vulnerablelogin.com",
      username: "vulnerable",
      password: "vulnerablepass",
    }),
  ];

  for (const loginInfo of breachedOrVulnerableLogins) {
    await Services.logins.addLoginAsync(loginInfo);
  }

  const loginDataSource = new LoginDataSource({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {},
    setLayout: () => {},
  });

  await TestUtils.waitForCondition(
    () => loginDataSource.doneReloadDataSource,
    "reloadDataSource has finished"
  );

  const vulnerableLogin = breachedOrVulnerableLogins.pop();
  const storageJSON = Services.logins.wrappedJSObject._storage;
  storageJSON.addPotentiallyVulnerablePassword(vulnerableLogin);

  const header = loginDataSource.enumerateLines().next().value;
  header.executeSortByAlerts();

  await TestUtils.waitForCondition(() => {
    return loginDataSource.doneReloadDataSource;
  }, "reloadDataSource has finished");

  const displayOriginAndUsername = getDisplayOriginAndUsername(
    loginDataSource.lines
  );

  const expectedLinesOrder = [
    {
      displayOrigin: "breached.com",
      username: "breached",
    },
    {
      displayOrigin: "vulnerablelogin.com",
      username: "vulnerable",
    },
  ];

  // When sorting by alerts, breached logins should appear first, then vulnerable logins, and then regular logins.
  // And within each category, logins should be sorted by displayOrigin (ignoring domain), then username, and then guid.
  Assert.deepEqual(
    expectedLinesOrder,
    displayOriginAndUsername,
    "lines were added in the correct order when sorting by alerts"
  );
});
