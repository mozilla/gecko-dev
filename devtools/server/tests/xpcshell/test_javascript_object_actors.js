/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

const { JSObjectsTestUtils, CONTEXTS } = ChromeUtils.importESModule(
  "resource://testing-common/JSObjectsTestUtils.sys.mjs"
);
JSObjectsTestUtils.init(this);

const EXPECTED_VALUES_FILE = "test_javascript_object_actors.snapshot.mjs";

/**
 * This test will run `test` function twice.
 * Once replicating a page debugging, and a second time replicating a worker debugging environment
 */
add_task(
  threadFrontTest(
    async function test({ threadFront, debuggee, _isWorkerServer }) {
      await JSObjectsTestUtils.runTest(
        EXPECTED_VALUES_FILE,
        async function ({ context, expression }) {
          // Only support basic JS Values
          if (context != CONTEXTS.JS) {
            return undefined;
          }

          // Create the function that the privileged code will call to pause
          // from executeOnNextTickAndWaitForPause callback
          debuggee.eval(`function stopMe(arg) { debugger; }`);

          const packet = await executeOnNextTickAndWaitForPause(async () => {
            let value;
            try {
              value = debuggee.eval(expression);
            } catch (e) {
              value = e;
            }

            // Catch all async rejection to avoid unecessary error reports
            if (value instanceof debuggee.Promise) {
              // eslint-disable-next-line max-nested-callbacks
              value.catch(function () {});
            }

            debuggee.stopMe(value);
          }, threadFront);

          const firstArg = packet.frame.arguments[0];

          await threadFront.resume();

          // Avoid storing any actor ID as it may not be super stable
          stripActorIDs(firstArg);

          return firstArg;
        }
      ); // End of runTest
    },

    // Use a content principal to better reflect evaluating into a web page,
    // but also to ensure seeing the stack trace of exception only within the sandbox
    // and especially not see the test harness ones, which are privileged.
    { principal: "https://example.org" }
  ) // End of threadFrontTest
);

function stripActorIDs(obj) {
  for (const name in obj) {
    if (name == "actor") {
      obj[name] = "<actor-id>";
    }
    if (typeof obj[name] == "object") {
      stripActorIDs(obj[name]);
    }
  }
}
