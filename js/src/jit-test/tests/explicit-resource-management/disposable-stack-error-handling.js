// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const errorToThrow = new Error("error");
  const stack = new DisposableStack();
  const obj = {
    [Symbol.dispose]() {
      disposed.push(1);
      throw errorToThrow;
    },
  };
  stack.use(obj);
  assertEq(stack.disposed, false);
  assertThrowsInstanceOf(() => stack.dispose(), Error);
  assertArrayEq(disposed, [1]);
  assertEq(stack.disposed, true);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3")];
  const stack = new DisposableStack();
  for (let i = 0; i < 3; i++) {
    stack.use({
      [Symbol.dispose]() {
        disposed.push(i);
        throw errorsToThrow[i];
      },
    });
  }
  assertEq(stack.disposed, false);
  assertSuppressionChain(() => stack.dispose(), errorsToThrow);
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [2, 1, 0]);
  stack.dispose();
  assertArrayEq(disposed, [2, 1, 0]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3"), new Error("error4")];
  function testStackDisposalWithUsingAndErrors() {
    const stack = new DisposableStack();
    for (let i = 0; i < 3; i++) {
      stack.use({
        [Symbol.dispose]() {
          disposed.push(i);
          throw errorsToThrow[i];
        },
      });
    }
    assertEq(stack.disposed, false);
    {
      using stk = stack;
      stk.use({
        [Symbol.dispose]() {
          disposed.push(3);
          throw errorsToThrow[3];
        },
      });
    }
  }
  assertSuppressionChain(testStackDisposalWithUsingAndErrors, errorsToThrow);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3")];
  function testStackDisposalWithUseAdoptDeferAndErrors() {
    const stack = new DisposableStack();
    stack.use({
      [Symbol.dispose]() {
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
    stack.dispose();
  }
  assertSuppressionChain(testStackDisposalWithUseAdoptDeferAndErrors, errorsToThrow);
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("error1"), new Error("error2"), new Error("error3"), new Error("error4")];
  function testStackDisposalWithUseAdoptDeferAndErrorsAndOutsideError() {
    using stack = new DisposableStack();
    stack.use({
      [Symbol.dispose]() {
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
  try {
    testStackDisposalWithUseAdoptDeferAndErrorsAndOutsideError();
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
