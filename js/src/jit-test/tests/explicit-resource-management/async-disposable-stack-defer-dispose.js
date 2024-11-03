// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  assertEq(stack.defer(() => disposed.push(1)), undefined);
  stack.defer(() => disposed.push(2));
  assertEq(stack.disposed, false);
  stack.disposeAsync();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [2, 1]);
  stack.disposeAsync();
  drainJobQueue();
  assertArrayEq(disposed, [2, 1]);
  assertThrowsInstanceOf(() => stack.defer(() => disposed.push(3)), ReferenceError);
  assertArrayEq(disposed, [2, 1]);
}

{
  const values = [];
  const stack = new AsyncDisposableStack();
  stack.defer(() => values.push(1));
  stack.defer(() => values.push(2));
  assertEq(stack.disposed, false);
  async function testDisposalsWithAwaitUsing() {
    {
      await using stk = stack;
      stk.use({
        [Symbol.asyncDispose]() {
          values.push(3);
        },
      });
      stk.adopt(4, (v) => values.push(v));
      stk.defer(() => values.push(5));
    }
  }
  testDisposalsWithAwaitUsing();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(values, [5, 4, 3, 2, 1]);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.defer(undefined);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.defer(null);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.defer(1);
  }, TypeError);
}
