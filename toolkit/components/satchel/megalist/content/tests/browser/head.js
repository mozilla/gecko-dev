/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

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
  await LoginTestUtils.addLogin({
    username: "bob",
    password: "pass1",
    origin: "https://example1.com",
  });
  await LoginTestUtils.addLogin({
    username: "sally",
    password: "pass2",
    origin: "https://example2.com",
  });
  await LoginTestUtils.addLogin({
    username: "ned",
    password: "pass3",
    origin: "https://example3.com",
  });

  for (let login of await Services.logins.getAllLogins()) {
    info(`Saved login: ${login.username}, ${login.password}, ${login.origin}`);
  }
}
