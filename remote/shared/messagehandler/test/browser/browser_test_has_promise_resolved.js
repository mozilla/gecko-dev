/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_promiseResolved() {
  const { promise, resolve } = Promise.withResolvers();

  ok(!(await hasPromiseResolved(promise)));
  resolve();
  ok(await hasPromiseResolved(promise));
});

add_task(async function test_promiseRejected() {
  const { promise, reject } = Promise.withResolvers();

  ok(!(await hasPromiseResolved(promise)));
  reject(new Error("Some message"));
  ok(await hasPromiseResolved(promise));

  await Assert.rejects(
    promise,
    e => e.message == "Some message",
    "Caught the expected error"
  );
});

add_task(async function test_asyncResolved() {
  const { promise, resolve } = Promise.withResolvers();
  async function simpleAsyncMethod() {
    // Drive this method from the external Promise.
    await promise;
  }

  const onSimpleAsyncMethod = simpleAsyncMethod();
  ok(!(await hasPromiseResolved(onSimpleAsyncMethod)));
  resolve();
  ok(await hasPromiseResolved(onSimpleAsyncMethod));
});

add_task(async function test_asyncRejected() {
  const { promise, resolve } = Promise.withResolvers();

  // This type of method used to be badly handled by hasPromiseResolved.
  // See Bug 1927144.
  async function asyncMethodErroring() {
    // Drive this method from the external Promise.
    await promise;
    throw new Error("Some message");
  }

  const onAsyncMethodErroring = asyncMethodErroring();
  ok(!(await hasPromiseResolved(onAsyncMethodErroring)));
  resolve();
  ok(await hasPromiseResolved(onAsyncMethodErroring));

  await Assert.rejects(
    onAsyncMethodErroring,
    e => e.message == "Some message",
    "Caught the expected error"
  );
});
