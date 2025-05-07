"use strict";

// Tests SpecialPowersForProcess spawn() in xpcshell tests.
// A browser chrome mochitest version of this test exists at
// testing/mochitest/tests/browser/browser_SpecialPowersForProcessSpawn.js

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

const { SpecialPowersForProcess } = ChromeUtils.importESModule(
  "resource://testing-common/SpecialPowersProcessActor.sys.mjs"
);

XPCShellContentUtils.init(this);

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com", "example.org"],
});
server.registerFile(
  "/file_xorigin_frames.html",
  do_get_file("file_xorigin_frames.html")
);

const scope = this;
const interceptedMessages = [];
add_setup(() => {
  const orig_do_report_result = scope.do_report_result;
  scope.do_report_result = (passed, msg, stack) => {
    if (msg?.startsWith?.("CHECK_THIS:")) {
      interceptedMessages.push(msg);
    }
    return orig_do_report_result(passed, msg, stack);
  };
  const orig_info = scope.info;
  scope.info = msg => {
    if (msg?.startsWith?.("CHECK_THIS:")) {
      interceptedMessages.push(msg);
    }
    return orig_info(msg);
  };

  registerCleanupFunction(() => {
    scope.do_report_result = orig_do_report_result;
    scope.info = orig_info;
  });
});

// Tests that SpecialPowersForProcess can spawn() in processes that the test
// grabbed off a contentPage, even after the original page navigated/closed.
add_task(async function test_SpecialPowersForProcess_spawn() {
  interceptedMessages.length = 0;

  const page = await XPCShellContentUtils.loadContentPage(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/file_xorigin_frames.html",
    { remote: true, remoteSubframes: true }
  );
  await page.spawn([], async () => {
    Assert.equal(
      await this.content.wrappedJSObject.loadedPromise,
      "frames_all_loaded",
      "All (cross-origin) frames have finished loading"
    );
  });
  const proc1 = page.browsingContext.children[0].currentWindowGlobal.domProcess;
  const proc2 = page.browsingContext.children[1].currentWindowGlobal.domProcess;
  Assert.equal(proc1, proc2, "The two child frames share the same process");

  const processBoundSpecialPowers = new SpecialPowersForProcess(scope, proc1);

  await page.spawn([], async () => {
    info("ContentPage.spawn: Change frame1 process");
    const frame1 = this.content.document.getElementById("frame1");
    Assert.throws(
      () => frame1.contentDocument.location.search,
      /TypeError: (can't access property "location", )?frame1.contentDocument is null/,
      "ContentPage.spawn: Assert, cannot read cross-origin content"
    );
    await new Promise(resolve => {
      frame1.onload = resolve;
      frame1.src = "/dummy?3";
    });
    // Verify that it is same-origin now.
    Assert.equal(
      frame1.contentDocument.location.search,
      "?3",
      "CHECK_THIS: ContentPage.spawn: Assert, frame1 is now same-origin"
    );
    info("CHECK_THIS: ContentPage.spawn: remove frame1");
    frame1.remove();

    // spawn() implementation has special logic to route Assert messages;
    // Prepare to check that Assert can be called after spawn() returns.
    this.content.assertAfterSpawnReturns = () => {
      Assert.ok(true, "CHECK_THIS: ContentPage.spawn: asssert after return");
    };
  });

  await page.spawn([], () => {
    this.content.assertAfterSpawnReturns();
  });

  Assert.equal(page.browsingContext.children.length, 1, "frame1 was removed");

  // Now frame1 has navigated (and switched processes) and removed, so if the
  // SpecialPowers implementation were to rely on JSWindowActor, then that
  // would break if we try to interact with it at this point. Check that we
  // can connect just fine (because JSProcessActor should be used instead).
  await processBoundSpecialPowers.spawn([], () => {
    info("CHECK_THIS: process-bound spawn: still works");
    Assert.equal(
      typeof content,
      "undefined",
      "CHECK_THIS: process-bound spawn: no content global"
    );
    // Need a shared object that outlives this SpecialPowersSandbox instance:
    const sharedGlobalObj = Cu.getGlobalForObject(Services);
    // spawn() implementation has special logic to route Assert messages;
    // Prepare to check that Assert can be called after spawn() returns.
    sharedGlobalObj.assertAfterProcessBoundSpawnReturns = () => {
      Assert.ok(true, "CHECK_THIS: process-bound spawn: asssert after return");
    };
  });
  await processBoundSpecialPowers.spawn([], () => {
    // Shared object that outlived the previous SpecialPowersSandbox instance:
    const sharedGlobalObj = Cu.getGlobalForObject(Services);
    sharedGlobalObj.assertAfterProcessBoundSpawnReturns();
    delete sharedGlobalObj.assertAfterProcessBoundSpawnReturns;
  });
  await page.close();
  await processBoundSpecialPowers.destroy();

  Assert.throws(
    () => processBoundSpecialPowers.spawn([], () => {}),
    /this.actor is null/,
    "Cannot spawn after destroy()"
  );

  const observedMessages = interceptedMessages.splice(0);
  Assert.deepEqual(
    observedMessages,
    [
      `CHECK_THIS: ContentPage.spawn: Assert, frame1 is now same-origin - "?3" == "?3"`,
      "CHECK_THIS: ContentPage.spawn: remove frame1",
      "CHECK_THIS: ContentPage.spawn: asssert after return - true == true",
      "CHECK_THIS: process-bound spawn: still works",
      `CHECK_THIS: process-bound spawn: no content global - "undefined" == "undefined"`,
      "CHECK_THIS: process-bound spawn: asssert after return - true == true",
    ],
    "Observed calls through spawn"
  );
});
