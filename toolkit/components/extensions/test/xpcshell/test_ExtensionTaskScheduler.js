/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionTaskScheduler } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionTaskScheduler.sys.mjs"
);

function assertIsPromise(value, message) {
  let t = typeof value == "object" && value && Cu.getClassName(value, true);
  equal(t, "Promise", message);
}

add_task(function synchronous_return_value() {
  const ets = new ExtensionTaskScheduler();
  const dummyId = "@not_an_extension";

  let read1 = ets.runReadTask(dummyId, () => 1);
  equal(read1, 1, "runReadTask returns return value");

  let write1 = ets.runWriteTask(dummyId, () => 1);
  equal(write1, 1, "runWriteTask returns return value");
});

add_task(async function test_throwing_task() {
  const ets = new ExtensionTaskScheduler();
  const dummyId = "@not_an_extension";

  function funcThrows() {
    throw new Error("funcThrows");
  }
  let rejectedPromise = Promise.reject(new Error("funcRejects"));
  function funcRejects() {
    return rejectedPromise;
  }

  Assert.throws(
    () => ets.runReadTask(dummyId, funcThrows),
    /funcThrows/,
    "sync task throws"
  );

  let rv1 = ets.runWriteTask(dummyId, funcRejects);
  equal(rv1, rejectedPromise, "Got original rejected promise for write task");
  let rv2 = ets.runReadTask(dummyId, funcThrows);
  assertIsPromise(rv2, "Got Promise for read task that is blocked on write");
  let calls = [];
  let rv3 = ets.runReadTask(dummyId, () => calls.push(1));
  assertIsPromise(rv3, "Got Promise for read task that does not reject");

  await Assert.rejects(rv1, /funcRejects/, "task resolves to rejection");
  await Assert.rejects(
    rv2,
    /funcThrows/,
    "task resolves to rejection for synchronously thrown error"
  );
  // The two await above run two rounds of microtasks, which should be plenty
  // to get rv1 to complete, and rv2 + rv3 to run in parallel, and rv3's sync
  // task to complete.
  Assert.deepEqual(calls, [1], "Next task still runs after rejection");
  equal(await rv3, 1, "Got result from non-rejecting task");
});

add_task(async function read_is_parallel__and_write_blocks_read() {
  const ets = new ExtensionTaskScheduler();
  const dummyId = "@not_an_extension";

  // This returns the internal ExtensionBoundTaskQueue instance. Since there is
  // no extension, it tests the core behavior of the ReadWriteQueue superclass.
  let q = ets.forExtensionId(dummyId);
  equal(q.hasPendingTasks(), false, "Task queue is empty");

  let calls = [];
  let delayedTask1 = Promise.withResolvers();
  let delayedTask2 = Promise.withResolvers();
  let delayedTask7 = Promise.withResolvers();

  let rv1 = q.runReadTask(() => calls.push(1) && delayedTask1.promise);
  equal(rv1, delayedTask1.promise, "runReadTask returns original promise (1)");
  equal(q.hasPendingTasks(), true, "Task queue is not empty");

  let rv2 = q.runReadTask(() => calls.push(2) && delayedTask2.promise);
  equal(rv2, delayedTask2.promise, "runReadTask returns original promise (2)");

  equal(
    q.runReadTask(() => 123),
    123,
    "runReadTask returns synchronous result immediately"
  );

  let rv3 = q.runWriteTask(() => calls.push(3));
  let rv4 = q.runReadTask(() => calls.push(4));
  let rv5 = q.runWriteTask(() => calls.push(5));
  let rv6 = q.runWriteTask(() => calls.push(6));
  let rv7 = q.runReadTask(() => calls.push(7) && delayedTask7.promise);

  Assert.deepEqual(
    calls,
    [1, 2],
    "Write task 3 awaits earlier read tasks and blocks later tasks"
  );

  delayedTask1.resolve("res1");
  delayedTask2.resolve("res2");
  equal(await rv1, "res1", "Got resolved value for task 1, read");
  equal(await rv2, "res2", "Got resolved value for task 2, read");
  Assert.deepEqual(
    calls,
    [1, 2, 3, 4, 5, 6, 7],
    "After unblocking the read task, all other non-async tasks ran immediately"
  );

  equal(
    q.runReadTask(() => 456),
    456,
    "Although a previous read task is still pending, it does not block new read"
  );

  equal(q.hasPendingTasks(), true, "Task queue is not empty");
  equal(ets._extensionBoundQueues.size, 1, "Queue is alive");
  equal(
    ets.forExtensionId(dummyId),
    q,
    "Same queue was reused despite the non-existing extension ID"
  );

  delayedTask7.resolve("res7");
  equal(await rv7, "res7", "Last read task ran");
  equal(ets._extensionBoundQueues.size, 0, "Queue is gone");
  equal(q.hasPendingTasks(), false, "Task queue is empty");

  let allReturnValues = [rv1, rv2, rv3, rv4, rv5, rv6, rv7];
  for (let [i, rv] of allReturnValues.entries()) {
    assertIsPromise(rv, `task ${i} returned a Promise`);
  }
  Assert.deepEqual(
    await Promise.all(allReturnValues),
    ["res1", "res2", 3, 4, 5, 6, "res7"],
    "All tasks resolved to the expected value"
  );
});

add_task(async function write_blocks_write() {
  const ets = new ExtensionTaskScheduler();
  const dummyId = "@not_an_extension";

  let q = ets.forExtensionId(dummyId);

  let calls = [];
  let delayedTask1 = Promise.withResolvers();
  let delayedTask2 = Promise.withResolvers();

  let rv1 = q.runWriteTask(() => calls.push(1) && delayedTask1.promise);
  equal(rv1, delayedTask1.promise, "runWriteTask (1) returns original promise");

  let rv2 = q.runWriteTask(() => calls.push(2) && delayedTask2.promise);
  notEqual(rv2, delayedTask2.promise, "runWriteTask (2) returns new Promise");
  Assert.deepEqual(calls, [1], "Second write blocked on first");

  delayedTask1.resolve("res1");
  delayedTask2.resolve("res2");
  equal(await rv1, "res1", "Got resolved value for task 1, write");
  equal(await rv2, "res2", "Got resolved value for task 2, write");

  equal(q.hasPendingTasks(), false, "Task queue is empty");
  equal(ets._extensionBoundQueues.size, 0, "Queue is gone");
});

// Tests that the internal task queues are not persisted in memory when the
// extensionId is not associated with a live extension.
add_task(async function schedule_non_existing_extension() {
  const ets = new ExtensionTaskScheduler();
  const nonExtensionId = "@does-not-exist";
  let q1 = ets.forExtensionId(nonExtensionId);
  let q2 = ets.forExtensionId(nonExtensionId);
  equal(q1, q2, "Returns same queue even if extension has not loaded yet");

  function triggerTask() {
    const funcReturns1 = () => 1;
    equal(ets.runReadTask(nonExtensionId, funcReturns1), 1, "Task ran");
  }
  equal(ets._extensionBoundQueues.size, 1, "Queue is there");
  triggerTask();
  equal(ets._extensionBoundQueues.size, 0, "Queue gone after running task");

  let q3 = ets.forExtensionId(nonExtensionId);
  notEqual(q1, q3, "Queue is different");

  equal(ets._extensionBoundQueues.size, 1, "Queue is there");
  triggerTask();
  equal(ets._extensionBoundQueues.size, 0, "Queue gone after running task");
});

add_task(async function schedule_existing_extension() {
  const ets = new ExtensionTaskScheduler();
  let extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();
  let extensionId = extension.id;

  function triggerTask() {
    const funcReturns1 = () => 1;
    equal(ets.runReadTask(extensionId, funcReturns1), 1, "Task ran");
  }

  let q1 = ets.forExtensionId(extensionId);
  let q2 = ets.forExtensionId(extensionId);
  equal(q1, q2, "Returns same queue for extension");

  equal(ets._extensionBoundQueues.size, 1, "Queue is there");
  triggerTask();
  equal(ets._extensionBoundQueues.size, 1, "Queue still there after task");

  let q3 = ets.forExtensionId(extensionId);
  equal(q1, q3, "Queue is the same");

  await extension.unload();
  equal(ets._extensionBoundQueues.size, 0, "Queue gone after extension unload");

  triggerTask();
  equal(ets._extensionBoundQueues.size, 0, "Queue still gone after task run");
});

add_task(async function load_and_unload_without_schedule() {
  const ets = new ExtensionTaskScheduler();
  let extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();
  let extensionId = extension.id;

  let q = ets.forExtensionId(extensionId);
  equal(q.hasPendingTasks(), false, "Task queue is empty");
  equal(ets._extensionBoundQueues.size, 1, "Queue is there");

  await extension.unload();
  equal(ets._extensionBoundQueues.size, 0, "Queue gone after extension unload");
});

add_task(async function queue_outlives_extension_with_pending_tasks() {
  const extensionId = "@extension_that_reloads";
  const ets = new ExtensionTaskScheduler();
  let extension;
  async function startExtension() {
    extension = ExtensionTestUtils.loadExtension({
      manifest: { browser_specific_settings: { gecko: { id: extensionId } } },
    });
    await extension.startup();
  }

  let q = ets.forExtensionId(extensionId);

  await startExtension();
  equal(ets._extensionBoundQueues.size, 1, "Queue still there");
  equal(ets.forExtensionId(extensionId), q, "Queue still same after startup");

  let delayedTask1 = Promise.withResolvers();
  let delayedTask2 = Promise.withResolvers();
  let rv1 = ets.runWriteTask(extensionId, () => delayedTask1.promise);
  let rv2 = ets.runReadTask(extensionId, () => delayedTask2.promise);
  notEqual(rv2, delayedTask2.promise, "read task blocked on write task");
  equal(ets._extensionBoundQueues.size, 1, "Queue is there after task");

  await extension.unload();
  equal(ets._extensionBoundQueues.size, 1, "Queue is there despite unload");

  await startExtension();
  delayedTask1.resolve("res1");
  delayedTask2.resolve("res2");
  equal(await rv1, "res1", "Write task finished");
  equal(await rv2, "res2", "Read task finished");
  equal(ets._extensionBoundQueues.size, 1, "Queue is there despite reload");

  equal(ets.forExtensionId(extensionId), q, "Queue still same after reload");

  await extension.unload();
  equal(ets._extensionBoundQueues.size, 0, "Queue not persisted after unload");
});
