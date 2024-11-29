/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ commands, threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const arg1 = packet.frame.arguments[0];
    Assert.equal(arg1.class, "Object");

    const objectFront = threadFront.pauseGrip(arg1);

    const obj1 = (
      await objectFront.getPropertyValue("obj1", null)
    ).value.return.getGrip();
    const obj2 = (
      await objectFront.getPropertyValue("obj2", null)
    ).value.return.getGrip();

    info(`Retrieve "context" function reference`);
    const context = (await objectFront.getPropertyValue("context", null)).value
      .return;
    info(`Retrieve "sum" function reference`);
    const sum = (await objectFront.getPropertyValue("sum", null)).value.return;
    info(`Retrieve "error" function reference`);
    const error = (await objectFront.getPropertyValue("error", null)).value
      .return;
    const notCallable = (
      await objectFront.getPropertyValue("notCallable", null)
    ).value.return;

    assert_response(await context.apply(obj1, [obj1]), {
      return: "correct context",
    });
    assert_response(await context.apply(obj2, [obj2]), {
      return: "correct context",
    });
    assert_response(await context.apply(obj1, [obj2]), {
      return: "wrong context",
    });
    assert_response(await context.apply(obj2, [obj1]), {
      return: "wrong context",
    });
    // eslint-disable-next-line no-useless-call
    assert_response(await sum.apply(null, [1, 2, 3, 4, 5, 6, 7]), {
      return: 1 + 2 + 3 + 4 + 5 + 6 + 7,
    });
    // eslint-disable-next-line no-useless-call
    assert_response(await error.apply(null, []), {
      throw: "an error",
    });

    try {
      await notCallable.apply(obj1, []);
      Assert.ok(false, "expected exception");
    } catch (err) {
      Assert.ok(!!err.message.match(/debugee object is not callable/));
    }

    await resume(threadFront);

    // In order to cover pausing from getPropertyValue we have to first resume
    // as pausing while already paused will be ignored.
    // So we have to have the pausingProp in a global object and access it while not paused.
    const { result: secondObjectFront } =
      await commands.scriptCommand.execute("obj");

    const onPropertyResumed = secondObjectFront.getPropertyValue(
      "pausingProp",
      null
    );

    // Ensure that we actually paused at the `debugger;` line.
    const packet2 = await waitForPause(threadFront);
    Assert.equal(packet2.frame.where.line, 18);
    Assert.equal(packet2.frame.where.column, 8);

    await threadFront.resume();
    await onPropertyResumed;
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    // These arguments are tested.
    // eslint-disable-next-line no-unused-vars
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  debuggee.eval(`
    stopMe({
      obj1: {},
      obj2: {},
      context(arg) {
        return this === arg ? "correct context" : "wrong context";
      },
      sum(...parts) {
        return parts.reduce((acc, v) => acc + v, 0);
      },
      error() {
        throw "an error";
      },
      notCallable: {},
    });
    var obj = {
      get pausingProp() {
        debugger;
      },
    };
  `);
}

function assert_response({ value }, expected) {
  assert_completion(value, expected);
}

function assert_completion(value, expected) {
  if (expected && "return" in expected) {
    assert_value(value.return, expected.return);
  }
  if (expected && "throw" in expected) {
    assert_value(value.throw, expected.throw);
  }
  if (!expected) {
    assert_value(value, expected);
  }
}

function assert_value(actual, expected) {
  Assert.equal(typeof actual, typeof expected);

  if (typeof expected === "object") {
    // Note: We aren't using deepEqual here because we're only doing a cursory
    // check of a few properties, not a full comparison of the result, since
    // the full outputs includes stuff like preview info that we don't need.
    for (const key of Object.keys(expected)) {
      assert_value(actual[key], expected[key]);
    }
  } else {
    Assert.equal(actual, expected);
  }
}
