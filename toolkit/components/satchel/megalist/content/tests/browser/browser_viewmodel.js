/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MegalistViewModel } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/MegalistViewModel.sys.mjs"
);

const EXPECTED_SNAPSHOTS_DATA = [
  // header data
  {
    lineIndex: 0,
    value: { total: 3, alerts: 0, statsTotal: 3 },
    field: undefined,
  },

  // first record
  { lineIndex: 1, value: "example1.com", field: "origin" },
  { lineIndex: 2, value: "bob", field: "username" },
  { lineIndex: 3, concealed: true, value: "••••••••", field: "password" },

  // second record
  { lineIndex: 4, value: "example2.com", field: "origin" },
  { lineIndex: 5, value: "sally", field: "username" },
  { lineIndex: 6, value: "••••••••", field: "password" },

  // third record
  { lineIndex: 7, value: "example3.com", field: "origin" },
  { lineIndex: 8, value: "ned", field: "username" },
  { lineIndex: 9, concealed: true, value: "••••••••", field: "password" },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contextual-password-manager.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });

  registerCleanupFunction(function () {
    LoginTestUtils.clearData();
  });
});

add_task(async function test_viewmodel_rebuildSnapshots() {
  const view = new MockView();
  const viewModel = new MegalistViewModel((message, args) =>
    view.messageFromViewModel(message, args)
  );

  info(
    "Check structure of snapshots received from view model after logins are added"
  );

  await addMockPasswords();

  await BrowserTestUtils.waitForCondition(
    () => view.snapshots.length === EXPECTED_SNAPSHOTS_DATA.length,
    "Received expected number of snapshots"
  );

  for (let snapshot of view.snapshots) {
    const { lineIndex } = snapshot;
    const expected = EXPECTED_SNAPSHOTS_DATA[lineIndex];
    is(snapshot.field, expected.field, "field property matches.");
    Assert.deepEqual(snapshot.value, expected.value, "value property matches.");

    if (snapshot.field === "password") {
      ok(snapshot.concealed, "password is concealed.");
    }
  }

  LoginTestUtils.clearData();
  viewModel.willDestroy();
});
