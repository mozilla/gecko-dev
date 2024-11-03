// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

let called = 0;

async function testDisposalMethodOnlyExtractedOnce() {
  await using x = {
    get [Symbol.asyncDispose]() {
      called++;
      return () => {}
    }
  }
}

testDisposalMethodOnlyExtractedOnce();
drainJobQueue();
assertEq(called, 1);
