// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const values = [];
  async function testAwaitUsingInForOfLoopHeadAwaitsPromise() {
    const obj = {
      [Symbol.asyncDispose]() {
        values.push(1);
        return new Promise(() => {});
      }
    };
  
    // The dispose operation here should wait forever.
    for (await using u of [obj]) {}
  
    values.push(2);
  }
  testAwaitUsingInForOfLoopHeadAwaitsPromise();
  drainJobQueue();
  assertArrayEq(values, [1]);
}

{
  const disposed = [];
  let caught;
  async function testAwaitUsingInForOfLoopHeadAwaitsPromiseRejection() {
    const obj = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
        return new Promise((res, rej) => {
          rej('err');
        });
      }
    };

    try {
      for (await using u of [obj]) {}
    } catch (e) {
      caught = e;
    }
  }
  testAwaitUsingInForOfLoopHeadAwaitsPromiseRejection();
  drainJobQueue();
  assertArrayEq(disposed, [1]);
  assertEq(caught, 'err');
}
