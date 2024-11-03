// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  let release;
  const p = new Promise((resolve) => {
    release = resolve;
  });
  async function testTryFinallyWithAwaitUsingWithAwaitsInTry() {
    try {
      await using x = {
        [Symbol.asyncDispose]() {
          disposed.push(1);
        }
      }
      await p;
      await using y = {
        [Symbol.asyncDispose]() {
          disposed.push(2);
        }
      }
    } finally {
      await using z = {
        [Symbol.asyncDispose]() {
          disposed.push(3);
        }
      }
    }
  }
  testTryFinallyWithAwaitUsingWithAwaitsInTry();
  release();
  drainJobQueue();
  assertArrayEq(disposed, [2, 1, 3]);
}

{
  const disposed = [];
  let release;
  const p = new Promise((resolve) => {
    release = resolve;
  });
  async function testTryFinallyWithAwaitUsingWithAwaitsInFinally() {
    try {
      await using x = {
        [Symbol.asyncDispose]() {
          disposed.push(1);
        }
      };
      await using y = {
        [Symbol.asyncDispose]() {
          disposed.push(2);
        }
      };
    } finally {
      await using z = {
        [Symbol.asyncDispose]() {
          disposed.push(3);
        }
      };
      await p;
      await using w = {
        [Symbol.asyncDispose]() {
          disposed.push(4);
        }
      }
    }
  }
  testTryFinallyWithAwaitUsingWithAwaitsInFinally();
  release();
  drainJobQueue();
  assertArrayEq(disposed, [2, 1, 4, 3]);
}

{
  const disposed = [];
  let val;
  async function testTryFinallyWithAwaitUsingAndReturnsInTry() {
    try {
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
      return 42;
    } finally {
      await using y = {
        [Symbol.asyncDispose]() {
          disposed.push(3);
        }
      }
    }
  }
  testTryFinallyWithAwaitUsingAndReturnsInTry().then((v) => {
    val = v;
  });
  drainJobQueue();
  assertEq(val, 42);
  assertArrayEq(disposed, [2, 1, 3]);
}

{
  const disposed = [];
  let val;
  async function testTryCatchFinallyWithAwaitUsingAndReturnsInCatch() {
    try {
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
      throw new Error("test");
    } catch (e) {
      await using z = {
        [Symbol.asyncDispose]() {
          disposed.push(3);
        }
      }
      return 42;
    } finally {
      await using y = {
        [Symbol.asyncDispose]() {
          disposed.push(4);
        }
      }
    }
  }
  testTryCatchFinallyWithAwaitUsingAndReturnsInCatch().then((v) => {
    val = v;
  });
  drainJobQueue();
  assertEq(val, 42);
  assertArrayEq(disposed, [2, 1, 3, 4]);
}

{
  const disposed = [];
  let val;
  async function testTryCatchFinallyWithAwaitUsingAndReturnsInFinally() {
    try {
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
      throw new Error("test");
    } catch (e) {
      await using z = {
        [Symbol.asyncDispose]() {
          disposed.push(3);
        }
      }
      return 42;
    } finally {
      await using y = {
        [Symbol.asyncDispose]() {
          disposed.push(4);
        }
      }
      return 43;
    }
  }
  testTryCatchFinallyWithAwaitUsingAndReturnsInFinally().then((v) => {
    val = v;
  });
  drainJobQueue();
  assertEq(val, 43);
  assertArrayEq(disposed, [2, 1, 3, 4]);
}
