// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const disposed = [];
async function testMixedDisposals() {
  using a = {
    [Symbol.dispose]: () => disposed.push("a")
  }

  await using b = {
    [Symbol.asyncDispose]: () => disposed.push("b")
  };

  {
    await using c = {
      [Symbol.dispose]: () => disposed.push("c")
    }

    using d = {
      [Symbol.dispose]: () => disposed.push("d")
    }
  }

  await using e = {
    [Symbol.asyncDispose]: () => disposed.push("e")
  };
}
testMixedDisposals();
drainJobQueue();
assertArrayEq(disposed, ["d", "c", "e", "b", "a"]);
