// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); error:Unhandled rejection: "sync"; --enable-explicit-resource-management

let fulfilled = false;
async function testSyncDisposeAwaitUsingNotAwaited() {
  const x = {
    [Symbol.dispose]: () => Promise.reject('sync')
  };

  for (await using d of [x]) {}
}
testSyncDisposeAwaitUsingNotAwaited().then(() => {
  fulfilled = true;
});
drainJobQueue();
// Returning a rejected from the sync dispose shouldn't reject.
assertEq(fulfilled, true);
