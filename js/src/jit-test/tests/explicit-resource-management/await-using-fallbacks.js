// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  let called = false;
  let calledSync = false;
  async function testCallsAsyncDispose() {
    await using x = {
      [Symbol.asyncDispose]() {
        called = true;
      },
      [Symbol.dispose]() {
        calledSync = true;
      }
    }
  }
  testCallsAsyncDispose();
  drainJobQueue();
  assertEq(called, true);
  assertEq(calledSync, false);
}

{
  const disposed = [];
  async function testCallsSyncDispose() {
    await using x = {
      [Symbol.dispose]() {
        disposed.push(1);
      }
    }

    await using y = {
      [Symbol.asyncDispose]: undefined,
      [Symbol.dispose]() {
        disposed.push(2);
      }
    }

    await using z = {
      [Symbol.asyncDispose]: null,
      [Symbol.dispose]() {
        disposed.push(3);
      }
    }
  }
  testCallsSyncDispose();
  drainJobQueue();
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  assertThrowsInstanceOf(() => {
    using x = {
      [Symbol.asyncDispose]() {
      }
    }
  }, TypeError);
}
