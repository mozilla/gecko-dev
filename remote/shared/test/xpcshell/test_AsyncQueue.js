/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const { AsyncQueue } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/AsyncQueue.sys.mjs"
);

function sleep(delay = 100) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, delay));
}

add_task(async function test_enqueueSyncTask() {
  let value = "";

  const queue = new AsyncQueue();
  await Promise.all([
    queue.enqueue(() => (value += "foo")),
    queue.enqueue(() => (value += "bar")),
  ]);

  equal(value, "foobar", "Tasks run in the correct order");
});

add_task(async function test_enqueueAsyncTask() {
  let value = "";

  const queue = new AsyncQueue();
  await Promise.all([
    queue.enqueue(async () => {
      await sleep(100);
      value += "foo";
    }),
    queue.enqueue(async () => {
      await sleep(10);
      value += "bar";
    }),
  ]);

  equal(value, "foobar", "Tasks run in the correct order");
});

add_task(async function test_enqueueAsyncTask() {
  let value = "";

  const queue = new AsyncQueue();
  const promises = Promise.all([
    queue.enqueue(async () => {
      await sleep(100);
      value += "foo";
    }),
    queue.enqueue(async () => {
      await sleep(10);
      value += "bar";
    }),
  ]);

  const promise = queue.enqueue(async () => (value += "42"));

  await promise;
  await promises;

  equal(value, "foobar42", "Tasks run in the correct order");
});

add_task(async function test_returnValue() {
  const queue = new AsyncQueue();
  const results = await Promise.all([
    queue.enqueue(() => "foo"),
    queue.enqueue(() => 42),
  ]);

  equal(results[0], "foo", "First task returned correct value");
  equal(results[1], 42, "Second task returned correct value");
});

add_task(async function test_enqueueErroneousTasks() {
  const queue = new AsyncQueue();

  await Assert.rejects(
    queue.enqueue(() => {
      throw new Error("invalid");
    }),
    /Error: invalid/,
    "Expected error was returned"
  );

  await Assert.rejects(
    queue.enqueue(async () => {
      throw new Error("invalid");
    }),
    /Error: invalid/,
    "Expected error was returned"
  );
});
