/* global MockAlertsService, registerAndWaitForActive */

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

async function setupServiceWorker(src, scope) {
  await NotificationTest.allowNotifications();
  await MockAlertsService.register();
  let registration = await registerAndWaitForActive(src, scope);
  SimpleTest.registerCleanupFunction(async () => {
    await registration.unregister();
  });
}

async function testFrame(src, args) {
  let { promise, resolve } = Promise.withResolvers();
  let iframe = document.createElement("iframe");
  let serialized = encodeURIComponent(JSON.stringify(args));
  iframe.src = `${src}?args=${serialized}`;
  window.callback = async function (data) {
    window.callback = null;
    document.body.removeChild(iframe);
    iframe = null;
    resolve(data);
  };
  document.body.appendChild(iframe);
  return await promise;
}
