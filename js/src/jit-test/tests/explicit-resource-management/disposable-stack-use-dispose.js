// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const stack = new DisposableStack();
  const obj = {
    [Symbol.dispose]() {
      disposed.push(1);
    },
  };
  stack.use(obj);
  assertEq(stack.disposed, false);
  assertEq(stack.dispose(), undefined);
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [1]);
  assertThrowsInstanceOf(() => stack.use(obj), ReferenceError);
}

{
  const disposed = [];
  const stack = new DisposableStack();
  for (let i = 0; i < 5; i++) {
    stack.use({
      [Symbol.dispose]() {
        disposed.push(i);
      },
    });
  }
  assertEq(stack.disposed, false);
  stack.dispose();
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [4, 3, 2, 1, 0]);
  stack.dispose();
  assertArrayEq(disposed, [4, 3, 2, 1, 0]);
}

{
  const disposed = [];
  const stack = new DisposableStack();
  for (let i = 0; i < 5; i++) {
    stack.use({
      [Symbol.dispose]() {
        disposed.push(i);
      },
    });
  }
  assertEq(stack.disposed, false);
  {
    using stk = stack;
  }
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [4, 3, 2, 1, 0]);
  {
    using stk2 = stack;
  }
  assertArrayEq(disposed, [4, 3, 2, 1, 0]);
}

{
  const disposed = [];
  const stack = new DisposableStack();
  for (let i = 0; i < 5; i++) {
    stack.use({
      [Symbol.dispose]() {
        disposed.push(i);
      },
    });
  }
  assertEq(stack.disposed, false);
  {
    using z = stack;
    z.use({
      [Symbol.dispose]() {
        disposed.push(5);
      },
    });
    assertEq(stack.disposed, false);
  }
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [5, 4, 3, 2, 1, 0]);
}

{
  const disposed = [];
  const stack = new DisposableStack();
  stack.use(undefined);
  stack.use(null);
  stack.use({
    [Symbol.dispose]() {
      disposed.push(1);
    }
  });
  stack.dispose();
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const stack = new DisposableStack();
  stack.use({
    [Symbol.dispose]() {
      disposed.push(1);
      stack.use({
        [Symbol.dispose]() {
          disposed.push(2);
        },
      });
    }
  });
  assertThrowsInstanceOf(() => stack.dispose(), ReferenceError);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.use(1);
  }, TypeError);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.use({
      [Symbol.dispose]: undefined
    });
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.use({
      [Symbol.dispose]: null
    });
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.use({
      [Symbol.dispose]: 1
    });
  }, TypeError);
}
