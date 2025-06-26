/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_handle_multi_action() {
  const action = {
    type: "MULTI_ACTION",
    data: {
      actions: [
        {
          type: "DISABLE_DOH",
        },
        {
          type: "OPEN_AWESOME_BAR",
        },
      ],
    },
  };
  const DOH_DOORHANGER_DECISION_PREF = "doh-rollout.doorhanger-decision";
  const NETWORK_TRR_MODE_PREF = "network.trr.mode";

  await SpecialPowers.pushPrefEnv({
    set: [
      [DOH_DOORHANGER_DECISION_PREF, "mochitest"],
      [NETWORK_TRR_MODE_PREF, 0],
    ],
  });

  await SMATestUtils.executeAndValidateAction(action);

  Assert.equal(
    Services.prefs.getStringPref(DOH_DOORHANGER_DECISION_PREF, ""),
    "UIDisabled",
    "Pref should be set on disabled"
  );
  Assert.equal(
    Services.prefs.getIntPref(NETWORK_TRR_MODE_PREF, 0),
    5,
    "Pref should be set on disabled"
  );

  Assert.ok(gURLBar.focused, "Focus should be on awesome bar");
});

add_task(async function test_handle_multi_action_with_invalid_action() {
  const action = {
    type: "MULTI_ACTION",
    data: {
      actions: [
        {
          type: "NONSENSE",
        },
      ],
    },
  };

  await SMATestUtils.validateAction(action);

  let error;
  try {
    await SpecialMessageActions.handleAction(action, gBrowser);
  } catch (e) {
    error = e;
  }

  ok(error, "should throw if an unexpected event is handled");
  Assert.equal(
    error.message,
    "Error in MULTI_ACTION event: Special message action with type NONSENSE is unsupported."
  );
});

add_task(async function test_multi_action_set_pref_ordered_execution() {
  const TEST_MULTI_PREF = "test.multi.pref";
  Services.prefs.clearUserPref(TEST_MULTI_PREF);

  let callOrder = [];
  const originalSetPref = SpecialMessageActions.setPref.bind(
    SpecialMessageActions
  );
  const originalHandleAction = SpecialMessageActions.handleAction.bind(
    SpecialMessageActions
  );

  sinon
    .stub(SpecialMessageActions, "handleAction")
    .callsFake(async function (action, browser) {
      if (action.type === "SET_PREF") {
        // Increase the delay for each subsequent item to ensure earlier items
        // would take longer to finish if they weren't executing sequentially
        let delay = 60 - callOrder.length * 10;
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(resolve => setTimeout(resolve, delay));
        callOrder.push(action.data.pref.value);
        originalSetPref(action.data.pref);
      }
      return originalHandleAction(action, browser);
    });

  const action = {
    type: "MULTI_ACTION",
    data: {
      orderedExecution: true,
      actions: [
        {
          type: "SET_PREF",
          data: { pref: { name: TEST_MULTI_PREF, value: "first" } },
        },
        {
          type: "SET_PREF",
          data: { pref: { name: TEST_MULTI_PREF, value: "second" } },
        },
        {
          type: "SET_PREF",
          data: { pref: { name: TEST_MULTI_PREF, value: "third" } },
        },
      ],
    },
  };

  Services.prefs.clearUserPref(`messaging-system-action.${TEST_MULTI_PREF}`);
  let browser = gBrowser || { ownerGlobal: window };
  let start = Date.now();
  await originalHandleAction(action, browser);
  let duration = Date.now() - start;
  // Total minimum delay: 60 + 50 + 20 = 130ms.
  Assert.ok(duration >= 130, "Ordered execution should take at least 130ms");
  Assert.deepEqual(
    callOrder,
    ["first", "second", "third"],
    "Actions executed sequentially"
  );
  Assert.equal(
    Services.prefs.getStringPref(`messaging-system-action.${TEST_MULTI_PREF}`),
    "third",
    "Pref was set"
  );
  Services.prefs.clearUserPref(TEST_MULTI_PREF);
  sinon.restore();
});
