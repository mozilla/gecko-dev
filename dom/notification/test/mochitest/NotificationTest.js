var NotificationTest = (function () {
  "use strict";

  function info(msg, name) {
    SimpleTest.info("::Notification Tests::" + (name || ""), msg);
  }

  function executeTests(tests, callback) {
    // context is `this` object in test functions
    // it can be used to track data between tests
    var context = {};

    (async function executeRemainingTests(remainingTests) {
      if (!remainingTests.length) {
        callback();
        return;
      }

      var nextTest = remainingTests.shift();
      var finishTest = executeRemainingTests.bind(null, remainingTests);
      var startTest = nextTest.call.bind(nextTest, context, finishTest);

      try {
        await startTest();
        // if no callback was defined for test function,
        // we must manually invoke finish to continue
        if (nextTest.length === 0) {
          finishTest();
        }
      } catch (e) {
        ok(false, `Test threw exception: ${e}`);
        finishTest();
      }
    })(tests);
  }

  // NotificationTest API
  return {
    run(tests) {
      SimpleTest.waitForExplicitFinish();

      addLoadEvent(async function () {
        executeTests(tests, function () {
          SimpleTest.finish();
        });
      });
    },

    allowNotifications() {
      return SpecialPowers.pushPermissions([
        {
          type: "desktop-notification",
          allow: SpecialPowers.Services.perms.ALLOW_ACTION,
          context: document,
        },
      ]);
    },

    denyNotifications() {
      return SpecialPowers.pushPermissions([
        {
          type: "desktop-notification",
          allow: SpecialPowers.Services.perms.DENY_ACTION,
          context: document,
        },
      ]);
    },

    clickNotification() {
      // TODO: how??
    },

    fireCloseEvent(title) {
      window.dispatchEvent(
        new CustomEvent("mock-notification-close-event", {
          detail: {
            title,
          },
        })
      );
    },

    info,

    payload: {
      body: "Body",
      tag: "fakeTag",
      icon: "icon.jpg",
      lang: "en-US",
      dir: "ltr",
    },
  };
})();
