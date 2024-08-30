/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

const { LoginBreaches } = ChromeUtils.importESModule(
  "resource:///modules/LoginBreaches.sys.mjs"
);

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

ChromeUtils.defineESModuleGetters(this, {
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
});

const BREACH_EXAMPLE = {
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

const BREACHED_LOGIN = LoginTestUtils.testData.formLogin({
  origin: "https://www.breached.com",
  formActionOrigin: "https://www.breached.com",
  username: "username",
  password: "password",
  timePasswordChanged: new Date("2018-12-15").getTime(),
});

const TEST_LOGIN_1 = LoginTestUtils.testData.formLogin({
  username: "bob",
  password: "pass1",
  origin: "https://example1.com",
});

const TEST_LOGIN_2 = LoginTestUtils.testData.formLogin({
  username: "sally",
  password: "pass2",
  origin: "https://example2.com",
});

const TEST_LOGIN_3 = LoginTestUtils.testData.formLogin({
  username: "ned",
  password: "pass3",
  origin: "https://example3.com",
});

const DEFAULT_MOCK_LOGINS = [TEST_LOGIN_1, TEST_LOGIN_2, TEST_LOGIN_3];

class MockView {
  #snapshots = [];

  get snapshots() {
    return this.#snapshots;
  }

  set snapshots(snapshots) {
    this.#snapshots = snapshots;
  }

  messageFromViewModel(message, args) {
    const functionName = `receive${message}`;
    this[functionName]?.(args);

    info(`${functionName} message received by view.`);
  }

  async receiveShowSnapshots({ snapshots }) {
    this.snapshots = snapshots;
  }
}

async function addMockPasswords() {
  info("Adding mock passwords");

  for (let login of DEFAULT_MOCK_LOGINS) {
    info(`Saving login: ${login.username}, ${login.password}, ${login.origin}`);
    await LoginTestUtils.addLogin(login);
  }
}

async function addBreach() {
  info("Adding breach");
  async function emitSync() {
    await RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).emit(
      "sync",
      { data: { current: [BREACH_EXAMPLE] } }
    );
  }

  gBrowserGlue.observe(null, "browser-glue-test", "add-breaches-sync-handler");
  const db = RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).db;
  await db.importChanges({}, Date.now(), [BREACH_EXAMPLE]);
  await emitSync();
}

async function openPasswordsSidebar() {
  info("Open Passwords sidebar");
  await SidebarController.show("viewMegalistSidebar");
  const sidebar = document.getElementById("sidebar");
  const megalist =
    sidebar.contentDocument.querySelector("megalist-alpha").shadowRoot;
  return megalist;
}

async function checkAllLoginsRendered(megalist) {
  info("Check that all logins are rendered.");
  const logins = await Services.logins.getAllLogins();
  await BrowserTestUtils.waitForCondition(() => {
    const passwordsList = megalist.querySelector(".passwords-list");
    return (
      passwordsList?.querySelectorAll("password-card").length === logins.length
    );
  }, "Password cards failed to render");

  ok(true, `${logins.length} password cards are rendered.`);
}
