// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  let promiseCalls = 0;
  class ExtendedPromise extends Promise {
    constructor(executor) {
      super(executor);
    }

    then(onFulfilled, onRejected) {
      promiseCalls++;
      return super.then(onFulfilled, onRejected);
    }
  }
  async function testAwaiUsingWithExtendedPromise() {
    await using x = {
      [Symbol.asyncDispose]: () => new ExtendedPromise((resolve) => {
        disposed.push(1);
        resolve();
      })
    }
    await using y = {
      [Symbol.asyncDispose]: () => new ExtendedPromise((resolve) => {
        disposed.push(2);
        resolve();
      })
    }
    await using z = {
      [Symbol.dispose]: () => new ExtendedPromise((resolve) => {
        disposed.push(3);
        resolve();
      })
    }
  }
  testAwaiUsingWithExtendedPromise();
  drainJobQueue();
  assertEq(promiseCalls, 2);
  assertArrayEq(disposed, [3, 2, 1]);
}
