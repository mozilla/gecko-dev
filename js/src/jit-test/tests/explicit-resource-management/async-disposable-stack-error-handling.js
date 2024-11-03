// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const errorToThrow = new Error("error");
  const stack = new AsyncDisposableStack();
  const obj = {
    [Symbol.asyncDispose]() {
      disposed.push(1);
      throw errorToThrow;
    },
  };
  stack.use(obj);
  assertEq(stack.disposed, false);
  assertThrowsInstanceOfAsync(async () => {
    await stack.disposeAsync();
  }, Error);
  assertArrayEq(disposed, [1]);
  assertEq(stack.disposed, true);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3")];
  const stack = new AsyncDisposableStack();
  for (let i = 0; i < 3; i++) {
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(i);
        throw errorsToThrow[i];
      },
    });
  }
  assertEq(stack.disposed, false);
  assertSuppressionChainAsync(async () => { await stack.disposeAsync() }, errorsToThrow);
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [2, 1, 0]);
  stack.disposeAsync();
  drainJobQueue();
  assertArrayEq(disposed, [2, 1, 0]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3"), new Error("error4")];
  async function testStackDisposalWithUsingAndErrors() {
    const stack = new AsyncDisposableStack();
    for (let i = 0; i < 3; i++) {
      stack.use({
        [Symbol.asyncDispose]() {
          disposed.push(i);
          throw errorsToThrow[i];
        },
      });
    }
    assertEq(stack.disposed, false);
    {
      await using stk = stack;
      stk.use({
        [Symbol.asyncDispose]() {
          disposed.push(3);
          throw errorsToThrow[3];
        },
      });
    }
  }
  assertSuppressionChainAsync(testStackDisposalWithUsingAndErrors, errorsToThrow);
  assertArrayEq(disposed, [3, 2, 1, 0]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3")];
  async function testStackDisposalWithUseAdoptDeferAndErrors() {
    const stack = new AsyncDisposableStack();
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      },
    });
    stack.adopt(2, (v) => {
      disposed.push(v);
      throw errorsToThrow[1];
    });
    stack.defer(() => {
      disposed.push(3);
      throw errorsToThrow[2];
    });
    assertEq(stack.disposed, false);
    await stack.disposeAsync();
  }
  assertSuppressionChainAsync(testStackDisposalWithUseAdoptDeferAndErrors, errorsToThrow);
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3"), new Error("error4")];
  async function testStackDisposalWithUseAdoptDeferAndErrorsAndOutsideError() {
    await using stack = new AsyncDisposableStack();
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      },
    });
    stack.adopt(2, (v) => {
      disposed.push(v);
      throw errorsToThrow[1];
    });
    stack.defer(() => {
      disposed.push(3);
      throw errorsToThrow[2];
    });

    throw errorsToThrow[3];
  }
  let caught = false;
  async function test() {
    try {
      await testStackDisposalWithUseAdoptDeferAndErrorsAndOutsideError();
    } catch (err) {
      caught = true;
      // the error thrown at function end would be suppressed and while
      // disposing the stack there would be another suppressed error.
      assertEq(err instanceof SuppressedError, true);
      assertEq(err.suppressed === errorsToThrow[3], true);
      assertSuppressionChain(() => { throw err.error }, [errorsToThrow[0], errorsToThrow[1], errorsToThrow[2]]);
    } finally {
      assertEq(caught, true);
    }
  }
  test();
  drainJobQueue();
  assertArrayEq(disposed, [3, 2, 1]);
}
