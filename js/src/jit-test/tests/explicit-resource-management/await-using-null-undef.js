// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  async function testAllowedNullAndUndefInitialisers() {
    await using x = null;
    await using y = undefined;
    await using z = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
      }
    }
  }
  testAllowedNullAndUndefInitialisers();
  drainJobQueue();
  assertArrayEq(disposed, [1]);
}
