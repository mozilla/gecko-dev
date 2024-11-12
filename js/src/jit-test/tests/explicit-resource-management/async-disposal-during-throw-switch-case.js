// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const disposed = [];
  let catchCalled = false;
  async function testDisposalInSwitchCaseDuringThrow(cs) {
    switch (cs) {
      case 1:
        await using a = {
          value: "a",
          [Symbol.asyncDispose]() {
            disposed.push(this.value);
          }
        };
        throw new CustomError();
    }
  }
  testDisposalInSwitchCaseDuringThrow(1).catch((e) => {
    catchCalled = true;
    assertEq(e instanceof CustomError, true);
  });
  drainJobQueue();
  assertEq(catchCalled, true);
  assertArrayEq(disposed, ["a"]);
}
