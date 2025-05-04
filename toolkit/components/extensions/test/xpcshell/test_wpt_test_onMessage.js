"use strict";

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

const WPT = "web-platform.test";
const server = createHttpServer({ hosts: [WPT, "example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

function loadPage(domain, query = "") {
  return ExtensionTestUtils.loadContentPage(
    `http://${domain}/data/file_browser.test.onMessage.html?${query}`
  );
}

async function requestAndCollect(unsubscribe = false) {
  let done = Promise.withResolvers();
  let messages = [];

  function onMessage({ detail }) {
    Assert.ok(true, "Received message: " + JSON.stringify(detail));
    messages.push(detail);
    if (detail.error || detail.done || detail.data.remainingTests === 0) {
      done.resolve(messages);
      this.content.removeEventListener("testMsg", onMessage);
    }
  }
  this.content.addEventListener("testMsg", onMessage);

  if (!unsubscribe) {
    this.content.wrappedJSObject.subscribe();
  } else {
    this.content.wrappedJSObject.unsubscribe();
  }
  return done.promise;
}

async function runTestExt() {
  // Intentionally not using ExtensionWrapper here, we don't want
  // browser.test.assertX() assertions to reach the test harness,
  // and explicitly check for expected messages in tests below.
  let ext = ExtensionTestCommon.generate({
    async background() {
      await browser.test.runTests([
        async () => {
          browser.test.assertEq(0xff, 255, "Same value.");
          // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
          await new Promise(r => setTimeout(r, 100));
          browser.test.assertTrue(1, "Truthy after await.");
        },
        function subTest2() {
          browser.test.assertThrows(() => {
            throw new Error("Wrong!");
          });
          browser.test.assertTrue(false, "Actually false.");
        },
        function still_runs_despite_previous_tast_failed() {
          browser.test.assertEq(true, "true", "No type coercion.");
        },
      ]);
      browser.test.sendMessage("test-ext-done");
    },
  });

  let done = Promise.withResolvers();

  // eslint-disable-next-line mozilla/balanced-listeners
  ext.on("test-message", (_, msg) => {
    if (msg === "test-ext-done") {
      done.resolve(ext);
    }
  });

  await Promise.all([ext.startup(), done.promise]);
  await ExtensionTestCommon.unloadTestExtension(ext);
}

function checkMessages(msgs, info) {
  Assert.deepEqual(
    msgs,
    [
      {
        message: "test-started",
        data: { testName: "unnamed_test_1" },
      },
      {
        message: "assert-equality",
        data: {
          result: true,
          message: "Same value.",
          expectedValue: "255",
          actualValue: "255",
        },
      },
      {
        message: "assert",
        data: { result: true, message: "Truthy after await." },
      },
      {
        message: "test-finished",
        data: { remainingTests: 2 },
      },
      {
        message: "test-started",
        data: { testName: "subTest2" },
      },
      {
        message: "assert",
        data: {
          result: true,
          message:
            "Function threw, expecting error to match '/.*/', got 'Error: Wrong!'",
        },
      },
      {
        message: "assert",
        data: { result: false, message: "Actually false." },
      },
      {
        message: "test-finished",
        data: { remainingTests: 1 },
      },
      {
        message: "test-started",
        data: { testName: "still_runs_despite_previous_tast_failed" },
      },
      {
        message: "assert-equality",
        data: {
          result: false,
          message: "No type coercion.",
          expectedValue: "true",
          actualValue: "true (different)",
        },
      },
      {
        message: "test-finished",
        data: { remainingTests: 0 },
      },
    ],
    `Expected messages - ${info}`
  );
}

// Normally this is not an expected situation. Real Firefox will always trigger
// loading ExtensionParent.sys.mjs on startup to support builtin extensions.
add_task(async function test_not_available_before_actor_init() {
  Assert.equal(
    false,
    Cu.isESModuleLoaded("resource://gre/modules/ExtensionParent.sys.mjs"),
    "ExtensionParent.sys.mjs not loaded in xpcshell tests by itself."
  );

  let page = await loadPage(WPT);
  let results = await page.spawn([], requestAndCollect);
  await page.close();

  Assert.deepEqual(
    results,
    [{ error: "Missing browser namespace." }],
    "browser.onMessage not defined before WPTMessages actor initializion."
  );
});

add_task(async function test_onMessage_queuing_scenarios() {
  // Importing to trigger the WPTMessages actor initialization.
  ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");

  info("Run test ext before onMessage listener subscribed.");
  await runTestExt();
  let page1 = await loadPage(WPT, 1);

  let results1 = await page1.spawn([], requestAndCollect);
  checkMessages(results1, "Correct messages queued.");

  info("Open second page with listener without closing the first one.");
  let page2 = await loadPage(WPT, 2);

  let results2 = page2.spawn([], requestAndCollect);

  await runTestExt();
  checkMessages(await results2, "Newest listener received messages.");

  let unsub1 = await page1.spawn([true], requestAndCollect);
  Assert.deepEqual(
    unsub1,
    [{ done: "unsubscribed" }],
    "Unsubscribing first listener shouldn't affect second."
  );

  let again2 = page2.spawn([], requestAndCollect);
  await runTestExt();
  checkMessages(await again2, "Second listener still received messages.");

  info("Closing both listeners.");
  await page1.close();
  await page2.close();

  info("Running the test before third listener subscribed.");
  await runTestExt();

  let page3 = await loadPage(WPT, 3);
  let results3 = await page3.spawn([], requestAndCollect);
  checkMessages(results3, "Third listener got queued messages.");
  await page3.close();
});

add_task(async function test_not_available_on_example_com() {
  // Importing to trigger the WPTMessages actor initialization.
  ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");

  let page = await loadPage("example.com");
  let results = await page.spawn([], requestAndCollect);
  await page.close();

  Assert.deepEqual(
    results,
    [{ error: "Missing browser namespace." }],
    "browser.onMessage not defined on example.com."
  );
});
