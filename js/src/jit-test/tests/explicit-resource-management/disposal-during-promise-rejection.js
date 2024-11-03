// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

let disposed = false;
async function testDisposalWithRejectedPromise() {
  using x = {
    [Symbol.dispose]() {
      disposed = true;
    }
  };
  await Promise.reject();
}

testDisposalWithRejectedPromise().catch(() => {});
drainJobQueue();
assertEq(disposed, true);
