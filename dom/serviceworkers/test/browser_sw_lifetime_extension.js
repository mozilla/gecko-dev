/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verify that ServiceWorkers interacting with each other can only set/extend
 * the lifetime of other ServiceWorkers to match their own lifetime, while
 * other clients that correspond to an open tab can provide fresh lifetime
 * extensions.  The specific scenario we want to ensure is impossible is two
 * ServiceWorkers interacting to keep each other alive indefinitely without the
 * involvement of a live tab.
 *
 * ### Test Machinery
 *
 * #### Determining Lifetimes
 *
 * In order to determine the lifetime deadline of ServiceWorkers, we have
 * exposed `lifetimeDeadline` on nsIServiceWorkerInfo.  This is a value
 * maintained exclusively by the ServiceWorkerManager on the
 * ServiceWorkerPrivate instances corresponding to each ServiceWorker.  It's not
 * something the ServiceWorker workers know, so it's appropriate to implement
 * this as a browser test with most of the logic in the parent process.
 *
 * #### Communicating with ServiceWorkers
 *
 * We use BroadcastChannel to communicate from a page in the test origin that
 * does not match any ServiceWorker scopes with the ServiceWorkers under test.
 * BroadcastChannel explicitly will not do anything to extend the lifetime of
 * the ServiceWorkers and is much simpler for us to use than trying to transfer
 * MessagePorts around since that would involve ServiceWorker.postMessage()
 * which will extend the ServiceWorker lifetime if used from a window client.
 *
 * #### Making a Service Worker that can Keep Updating
 *
 * ServiceWorker update checks do a byte-wise comparison; if the underlying
 * script/imports have not changed, the update process will be aborted.  So we
 * use an .sjs script that generates a script payload that has a "version" that
 * updates every time the script is fetched.
 *
 * Note that one has to be careful with an .sjs like that because
 * non-subresource fetch events will automatically run a soft update check, and
 * functional events will run a soft update if the registration is stale.  We
 * never expect the registration to be stale in our tests because 24 hours won't
 * have passed, but page navigation is obviously a very common testing thing.
 * We ensure we don't perform any intercepted navigations.
 *
 * To minimize code duplication, we have that script look like:
 * ```
 * var version = ${COUNTER};
 * importScripts("sw_inter_sw_postmessage.js");
 * ```
 */

/* import-globals-from browser_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/serviceworkers/test/browser_head.js",
  this
);

const TEST_ORIGIN = "https://test1.example.org";

/**
 * Install equivalent ServiceWorkers on 2 scopes that will message each other
 * on request via BroadcastChannel message, verifying that the ServiceWorkers
 * cannot extend each other's deadlines beyond their own deadline.
 */
async function test_post_message_between_service_workers() {
  info("## Installing the ServiceWorkers");
  const aSwDesc = {
    origin: TEST_ORIGIN,
    scope: "sw-a",
    script: "sw_inter_sw_postmessage.js?a",
  };
  const bSwDesc = {
    origin: TEST_ORIGIN,
    scope: "sw-b",
    script: "sw_inter_sw_postmessage.js?b",
  };

  // Wipe the origin for cleanup; this will remove the registrations too.
  registerCleanupFunction(async () => {
    await clear_qm_origin_group_via_clearData(TEST_ORIGIN);
  });

  const aReg = await install_sw(aSwDesc);
  const bReg = await install_sw(bSwDesc);

  info("## Terminating the ServiceWorkers");
  // We always want to wait for the workers to be fully terminated because they
  // listen for our BroadcastChannel messages and until ServiceWorkers are no
  // longer owned by the main thread, a race is possible if we don't wait.
  const aSWInfo = aReg.activeWorker;
  await aSWInfo.terminateWorker();

  const bSWInfo = bReg.activeWorker;
  await bSWInfo.terminateWorker();

  is(aSWInfo.lifetimeDeadline, 0, "SW A not running.");
  is(bSWInfo.lifetimeDeadline, 0, "SW B not running.");
  is(aSWInfo.launchCount, 1, "SW A did run once, though.");
  is(bSWInfo.launchCount, 1, "SW B did run once, though.");

  info("## Beginning PostMessage Checks");
  let testStart = Cu.now();

  const { closeHelperTab, postMessageScopeAndWaitFor, broadcastAndWaitFor } =
    await createMessagingHelperTab(TEST_ORIGIN, "inter-sw-postmessage");
  registerCleanupFunction(closeHelperTab);

  // - Have the helper page postMessage SW A to spawn it, waiting for SW A to report in.
  await postMessageScopeAndWaitFor(
    "sw-a",
    "Hello, the contents of this message don't matter!",
    "a:received-post-message-from:wc-helper"
  );

  let aLifetime = aSWInfo.lifetimeDeadline;
  Assert.greater(
    aLifetime,
    testStart,
    "SW A should be running with a deadline in the future."
  );
  is(bSWInfo.lifetimeDeadline, 0, "SW B not running.");
  is(aSWInfo.launchCount, 2, "SW A was launched by our postMessage.");
  is(bSWInfo.launchCount, 1, "SW B has not been re-launched yet.");

  // - Ask SW A to postMessage SW B, waiting for SW B to report in.
  await broadcastAndWaitFor(
    "a:post-message-to:reg-sw-b",
    "b:received-post-message-from:sw-a"
  );

  is(
    bSWInfo.lifetimeDeadline,
    aLifetime,
    "SW B has same deadline as SW A after cross-SW postMessage"
  );

  // - Ask SW B to postMessage SW A, waiting for SW A to report in.
  await broadcastAndWaitFor(
    "b:post-message-to:reg-sw-a",
    "a:received-post-message-from:sw-b"
  );

  is(
    bSWInfo.lifetimeDeadline,
    aLifetime,
    "SW A still has the same deadline after B's cross-SW postMessage"
  );
  is(bSWInfo.launchCount, 2, "SW B was re-launched.");
  is(aSWInfo.lifetimeDeadline, aLifetime, "SW A deadline unchanged");
  is(aSWInfo.launchCount, 2, "SW A launch count unchanged.");

  // - Have the helper page postMessage SW B, waiting for B to report in.
  await postMessageScopeAndWaitFor(
    "sw-b",
    "Hello, the contents of this message don't matter!",
    "b:received-post-message-from:wc-helper"
  );
  let bLifetime = bSWInfo.lifetimeDeadline;
  Assert.greater(
    bLifetime,
    aLifetime,
    "SW B should have a deadline after A's after the page postMessage"
  );
  is(aSWInfo.lifetimeDeadline, aLifetime, "SW A deadline unchanged");
  is(aSWInfo.launchCount, 2, "SW A launch count unchanged.");

  // - Have SW B postMessage SW A, waiting for SW A to report in.
  await broadcastAndWaitFor(
    "b:post-message-to:reg-sw-a",
    "a:received-post-message-from:sw-b"
  );
  is(
    aSWInfo.lifetimeDeadline,
    bLifetime,
    "SW A should have the same deadline as B after B's cross-SW postMessage"
  );
  is(aSWInfo.launchCount, 2, "SW A launch count unchanged.");
  is(bSWInfo.lifetimeDeadline, bLifetime, "SW B deadline unchanged");
  is(bSWInfo.launchCount, 2, "SW B launch count unchanged.");
}
add_task(test_post_message_between_service_workers);

/**
 * Install a ServiceWorker that will update itself on request via
 * BroadcastChannel message and verify that the lifetimes of the new updated
 * ServiceWorker are the same as the requesting ServiceWorker.  We also want to
 * verify that a request to update from a page gets a fresh lifetime.
 */
async function test_eternally_updating_service_worker() {
  info("## Installing the Eternally Updating ServiceWorker");
  const swDesc = {
    origin: TEST_ORIGIN,
    scope: "sw-u",
    script: "sw_always_updating_inter_sw_postmessage.sjs?u",
  };

  // Wipe the origin for cleanup; this will remove the registrations too.
  registerCleanupFunction(async () => {
    await clear_qm_origin_group_via_clearData(TEST_ORIGIN);
  });

  let testStart = Cu.now();

  const reg = await install_sw(swDesc);
  const firstInfo = reg.activeWorker;
  const firstLifetime = firstInfo.lifetimeDeadline;

  Assert.greater(
    firstLifetime,
    testStart,
    "The first generation should be running with a deadline in the future."
  );

  const { closeHelperTab, broadcastAndWaitFor, updateScopeAndWaitFor } =
    await createMessagingHelperTab(TEST_ORIGIN, "inter-sw-postmessage");
  registerCleanupFunction(closeHelperTab);

  info("## Beginning Self-Update Requests");

  // - Ask 1st gen SW to update the reg, 2nd gen SW should have same lifetime.
  await broadcastAndWaitFor("u#1:update-reg:sw-u", "u:version-activated:2");

  // We don't have to worry about async races here because these state changes
  // are authoritative on the parent process main thread, which is where we are
  // running; we can only have heard about the activation via BroadcastChannel
  // after the state has updated.
  const secondInfo = reg.activeWorker;
  const secondLifetime = secondInfo.lifetimeDeadline;
  is(firstLifetime, secondLifetime, "Version 2 has same lifetime as 1.");

  // - Ask 2nd gen SW to update the reg, 3rd gen SW should have same lifetime.
  await broadcastAndWaitFor("u#2:update-reg:sw-u", "u:version-activated:3");

  const thirdInfo = reg.activeWorker;
  const thirdLifetime = thirdInfo.lifetimeDeadline;
  is(firstLifetime, thirdLifetime, "Version 3 has same lifetime as 1 and 2.");

  // - Ask the helper page to update the reg, 4th gen SW should have fresh life.
  await updateScopeAndWaitFor("sw-u", "u:version-activated:4");

  const fourthInfo = reg.activeWorker;
  const fourthLifetime = fourthInfo.lifetimeDeadline;
  Assert.greater(
    fourthLifetime,
    firstLifetime,
    "Version 4 has a fresh lifetime."
  );

  // - Ask 4th gen SW to update the reg, 5th gen SW should have same lifetime.
  await broadcastAndWaitFor("u#4:update-reg:sw-u", "u:version-activated:5");

  const fifthInfo = reg.activeWorker;
  const fifthLifetime = fifthInfo.lifetimeDeadline;
  is(fourthLifetime, fifthLifetime, "Version 5 has same lifetime as 4.");
}
add_task(test_eternally_updating_service_worker);

/**
 * Install a ServiceWorker that will create a new registration and verify that
 * the lifetime for the new ServiceWorker being installed for the new
 * registration is the same as the requesting ServiceWorker.
 */
async function test_service_worker_creating_new_registrations() {
  info("## Installing the Bootstrap ServiceWorker");
  const cSwDesc = {
    origin: TEST_ORIGIN,
    scope: "sw-c",
    script: "sw_inter_sw_postmessage.js?c",
  };

  // Wipe the origin for cleanup; this will remove the registrations too.
  registerCleanupFunction(async () => {
    await clear_qm_origin_group_via_clearData(TEST_ORIGIN);
  });

  let testStart = Cu.now();

  const cReg = await install_sw(cSwDesc);
  const cSWInfo = cReg.activeWorker;
  const cLifetime = cSWInfo.lifetimeDeadline;

  Assert.greater(
    cLifetime,
    testStart,
    "The bootstrap registration worker should be running with a deadline in the future."
  );

  const { closeHelperTab, broadcastAndWaitFor, updateScopeAndWaitFor } =
    await createMessagingHelperTab(TEST_ORIGIN, "inter-sw-postmessage");
  registerCleanupFunction(closeHelperTab);

  info("## Beginning Propagating Registrations");

  // - Ask the SW to install a ServiceWorker at scope d
  await broadcastAndWaitFor("c:install-reg:d", "d:version-activated:0");

  const dSwDesc = {
    origin: TEST_ORIGIN,
    scope: "sw-d",
    script: "sw_inter_sw_postmessage.js?d",
  };
  const dReg = swm_lookup_reg(dSwDesc);
  ok(dReg, "found the new 'd' registration");
  const dSWInfo = dReg.activeWorker;
  ok(dSWInfo, "The 'd' registration has the expected active worker.");
  const dLifetime = dSWInfo.lifetimeDeadline;
  is(
    dLifetime,
    cLifetime,
    "The new worker has the same lifetime as the worker that triggered its installation."
  );
}
add_task(test_service_worker_creating_new_registrations);
