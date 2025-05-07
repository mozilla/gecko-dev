"use strict";

// Tests SpecialPowersForProcess spawn() in browser chrome mochitests.
// An xpcshell version of this test exists at
// testing/mochitest/tests/Harness_sanity/test_SpecialPowersForProcessSpawn.js

const { SpecialPowersForProcess } = ChromeUtils.importESModule(
  "resource://testing-common/SpecialPowersProcessActor.sys.mjs"
);

const scope = this;
const interceptedMessages = [];
add_setup(() => {
  const orig_record = SimpleTest.record;
  SimpleTest.record = (condition, name, diag, stack, expected) => {
    if (name?.startsWith?.("CHECK_THIS:")) {
      interceptedMessages.push(name);
    }
    return orig_record(condition, name, diag, stack, expected);
  };
  const orig_info = SimpleTest.info;
  SimpleTest.info = msg => {
    if (msg?.startsWith?.("CHECK_THIS:")) {
      interceptedMessages.push(msg);
    }
    return orig_info(msg);
  };
  registerCleanupFunction(() => {
    SimpleTest.record = orig_record;
    SimpleTest.info = orig_info;
  });
});

// Tests that SpecialPowersForProcess can spawn() in processes that the test
// grabbed off a contentPage, even after the original page navigated/closed.
add_task(async function test_SpecialPowersForProcess_spawn() {
  interceptedMessages.length = 0;

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/testing/mochitest/tests/browser/file_xorigin_frames.html"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    Assert.equal(
      await this.content.wrappedJSObject.loadedPromise,
      "frames_all_loaded",
      "All (cross-origin) frames have finished loading"
    );
  });
  const browsingContext = tab.linkedBrowser.browsingContext;
  const proc1 = browsingContext.children[0].currentWindowGlobal.domProcess;
  const proc2 = browsingContext.children[1].currentWindowGlobal.domProcess;
  Assert.equal(proc1, proc2, "The two child frames share the same process");

  const processBoundSpecialPowers = new SpecialPowersForProcess(scope, proc1);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
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

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    this.content.assertAfterSpawnReturns();
  });

  Assert.equal(browsingContext.children.length, 1, "frame1 was removed");

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
  BrowserTestUtils.removeTab(tab);
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
