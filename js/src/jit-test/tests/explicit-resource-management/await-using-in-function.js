// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

{
  const disposed = [];
  async function testAwaitUsingInFunction() {
    await using x = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
      }
    }

    await using y = {
      [Symbol.asyncDispose]() {
        disposed.push(2);
      }
    }

    await using z = {
      [Symbol.asyncDispose]() {
        disposed.push(3);
      }
    }
  }
  testAwaitUsingInFunction();
  assertArrayEq(disposed, [3]);
  drainJobQueue();
  assertArrayEq(disposed, [3, 2, 1]);
}
