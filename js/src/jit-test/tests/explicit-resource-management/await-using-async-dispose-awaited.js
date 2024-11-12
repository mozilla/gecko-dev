// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

let catchCalled = false;
async function testAsyncDisposeAwaitUsingAwaited() {
  await using x = {
    [Symbol.asyncDispose]: () => Promise.reject('async')
  };
}
testAsyncDisposeAwaitUsingAwaited().catch((e) => {
  catchCalled = true;
  assertEq(e, 'async');
});
drainJobQueue();
assertEq(catchCalled, true);
