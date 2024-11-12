// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  let thenCalled = false;
  async function testReturnValueIsPreserved() {
    await using a = {
      [Symbol.asyncDispose]: () => disposed.push("a")
    };
    await using b = {
      [Symbol.asyncDispose]: () => disposed.push("b")
    };
    return 200;
  }
  testReturnValueIsPreserved().then((val) => {
    thenCalled = true;
    assertEq(val, 200);
  });
  drainJobQueue();
  assertEq(thenCalled, true);
  assertArrayEq(disposed, ["b", "a"]);
}

{
  const disposed = [];
  let thenCalled = false;
  async function testReturnValuePreservedWhenWithFinally() {
    try {
      await using a = {
        [Symbol.asyncDispose]: () => disposed.push("a")
      }
      return 1;
    } finally {
      await using b = {
        [Symbol.asyncDispose]: () => disposed.push("b")
      }
      return 2;
    }
  }
  testReturnValuePreservedWhenWithFinally().then((val) => {
    thenCalled = true;
    assertEq(val, 2);
  });
  drainJobQueue();
  assertEq(thenCalled, true);
  assertArrayEq(disposed, ["a", "b"]);
}
