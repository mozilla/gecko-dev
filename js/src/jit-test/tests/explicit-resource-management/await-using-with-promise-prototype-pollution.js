// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  let thenCalled = 0;
  const originalThen = Promise.prototype.then;
  Promise.prototype.then = function (onFulfilled, onRejected) {
    thenCalled++;
    return originalThen.call(this, onFulfilled, onRejected);
  };
  const thenCalls = [];
  async function testAwaitUsingWithPromisePrototypePollution() {
    await using x = {
      [Symbol.asyncDispose]() {
        return new Promise((resolve) => {
          disposed.push(1);
          resolve(1);
        }).then((r) => {
          thenCalls.push(r);
        });
      }
    }
    await using y = {
      [Symbol.asyncDispose]() {
        return new Promise((resolve) => {
          disposed.push(2);
          resolve();
        });
      }
    }
  }
  testAwaitUsingWithPromisePrototypePollution();
  drainJobQueue();
  assertEq(thenCalled, 1);
  assertArrayEq(disposed, [2, 1]);
  assertArrayEq(thenCalls, [1]);
}
