// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  async function testDisposalsWithAsyncDisposableStack() {
    const obj = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
      },
    };
    stack.use(obj);
    assertEq(stack.disposed, false);
    assertEq(await stack.disposeAsync(), undefined);
  }
  testDisposalsWithAsyncDisposableStack();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [1]);

  // `use` should throw if the stack is already disposed.
  assertThrowsInstanceOf(() => stack.use({ [Symbol.asyncDispose]: () => {} }), ReferenceError);
}

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  async function testAsyncDisposableStackMultipleDispose() {
    for (let i = 0; i < 5; i++) {
      stack.use({
        [Symbol.dispose]() {
          disposed.push(i);
        },
      });
    }
    assertEq(stack.disposed, false);
    await stack.disposeAsync();

    // Calling again shouldn't do anything.
    await stack.disposeAsync();
  }
  testAsyncDisposableStackMultipleDispose();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [4, 3, 2, 1, 0]);
}

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  async function testAsyncDisposableStackUseWithFallback() {
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(1);
      }
    });
    stack.use({
      [Symbol.dispose]() {
        disposed.push(2);
      }
    });
    stack.use({
      [Symbol.asyncDispose]: undefined,
      [Symbol.dispose]() {
        disposed.push(3);
      }
    });
    stack.use({
      [Symbol.asyncDispose]: null,
      [Symbol.dispose]() {
        disposed.push(4);
      }
    });
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(5);
      },
      [Symbol.dispose]() {
        // This shouldn't be called because @@asyncDispose exists.
        disposed.push(6);
      }
    });
    assertEq(stack.disposed, false);
    await stack.disposeAsync();
  }
  testAsyncDisposableStackUseWithFallback();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [5, 4, 3, 2, 1]);
}

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  for (let i = 0; i < 5; i++) {
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(i);
      },
    });
  }
  assertEq(stack.disposed, false);
  async function testAsyncDisposableStackWithUsingDecl() {
    {
      await using stk = stack;
      stk.use({
        [Symbol.asyncDispose]() {
          disposed.push(5);
        },
      });
      stk.use({
        [Symbol.dispose]() {
          disposed.push(6);
        }
      });
    }
    assertEq(stack.disposed, true);
    {
      // This should be no-op.
      await using stk2 = stack;
    }
    assertEq(stack.disposed, true);
  }
  testAsyncDisposableStackWithUsingDecl();
  drainJobQueue();
  assertArrayEq(disposed, [6, 5, 4, 3, 2, 1, 0]);
}

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  async function testAsyncDisposableStackWithNullUndefineds() {
    stack.use(undefined);
    stack.use(null);
    stack.use({
      [Symbol.asyncDispose]() {
        disposed.push(1);
      }
    });
    await stack.disposeAsync();
  }
  testAsyncDisposableStackWithNullUndefineds();
  drainJobQueue();
  assertArrayEq(disposed, [1]);
  assertEq(stack.disposed, true);
}

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  let error;
  stack.use({
    [Symbol.asyncDispose]() {
      disposed.push(1);

      // This method is called during disposal, and before the
      // disposal process is started the stack's state is set to disposed.
      // Thus, calling stack use here again would throw.
      stack.use({
        [Symbol.asyncDispose]() {
          disposed.push(2);
        },
      });
    }
  });
  stack.disposeAsync().catch((e) => {
    error = e;
  });
  drainJobQueue();
  assertEq(error instanceof ReferenceError, true);
  assertArrayEq(disposed, [1]);
  assertEq(stack.disposed, true);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.use(1);
  }, TypeError);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.use({
      [Symbol.asyncDispose]: undefined
    });
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.use({
      [Symbol.asyncDispose]: null
    });
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.use({
      [Symbol.asyncDispose]: 1
    });
  }, TypeError);
}
