/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_PIN_FIREFOX_TO_START_MENU() {
  const sandbox = sinon.createSandbox();
  let shell = {
    QueryInterface: () => shell,
    get shellService() {
      return this;
    },

    isCurrentAppPinnedToStartMenuAsync: sandbox.stub(),
    pinCurrentAppToStartMenuAsync: sandbox.stub().resolves(true),
  };

  // Prefer the mocked implementation and fall back to the original version,
  // which can call back into the mocked version (via this.shellService).
  shell = new Proxy(shell, {
    get(target, prop) {
      return (prop in target ? target : ShellService)[prop];
    },
  });

  shell.isCurrentAppPinnedToStartMenuAsync.resolves(false);
  const test = () =>
    SMATestUtils.executeAndValidateAction(
      { type: "PIN_FIREFOX_TO_START_MENU" },
      {
        ownerGlobal: {
          getShellService: () => shell,
        },
      }
    );

  shell.isCurrentAppPinnedToStartMenuAsync.resolves(false);
  await test();

  function check(count, message) {
    Assert.equal(
      shell.pinCurrentAppToStartMenuAsync.callCount,
      count,
      `pinCurrentAppToStartMenuAsync was ${message} by the action for Windows`
    );
  }
  check(1, "called");

  // Pretend the app is already pinned.
  shell.isCurrentAppPinnedToStartMenuAsync.resolves(true);
  await test();
  check(1, "not called");

  // Pretend the app became unpinned.
  shell.isCurrentAppPinnedToStartMenuAsync.resolves(false);
  await test();
  check(2, "called again");
});
