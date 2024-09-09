// |reftest| shell-option(--enable-promise-try) skip-if(!Promise.try||!xulRuntime.shell)
{
  // Check the value of `this` in the callback is undefined.
  let this_ = {};
  Promise.try(function () {
    "use strict";
    this_ = this;
  });
  drainJobQueue();
  assertEq(this_, undefined);
}

{
  // Error when the `resolve` function passed by a custom promise subclass throws.
  let resolveCalled = false;
  let rejectCalled = false;
  let expectedError = new Error("foo");

  class CustomPromise {
    constructor(executor) {
      let resolve = () => {
        resolveCalled = true;
        throw expectedError;
      };
      let reject = () => rejectCalled = true;
      executor(resolve, reject);
    }
  }

  let errorThrown = undefined;
  try {
    Promise.try.call(CustomPromise, function () {});
  } catch (e) {
    errorThrown = e;
  }
  drainJobQueue();

  assertEq(resolveCalled, true);
  assertEq(rejectCalled, false);
  assertEq(errorThrown, expectedError);
}

{
  // Error when the `reject` function passed by a custom promise subclass throws.
  let resolveCalled = false;
  let rejectCalled = false;
  let expectedError = new Error("foo");

  class CustomPromise {
    constructor(executor) {
      let resolve = () => {
        resolveCalled = true;
      };
      let reject = () => {
        rejectCalled = true;
        throw expectedError;
      };
      executor(resolve, reject);
    }
  }

  let errorThrown = undefined;
  try {
    Promise.try.call(CustomPromise, function () { throw new Error("bar") });
  } catch (e) {
    errorThrown = e;
  }
  drainJobQueue();

  assertEq(resolveCalled, false);
  assertEq(rejectCalled, true);
  assertEq(errorThrown, expectedError);
}

reportCompare(true, true);
