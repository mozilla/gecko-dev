// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

{
  const values = [];
  const stack = new AsyncDisposableStack();
  assertEq(stack.adopt(1, (v) => values.push(v)), 1);
  const obj = { value: 2 };
  assertEq(stack.adopt(obj, (v) => values.push(v.value)), obj);
  assertEq(stack.disposed, false);
  stack.disposeAsync();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(values, [2, 1]);
  stack.disposeAsync();
  drainJobQueue();
  assertArrayEq(values, [2, 1]);
  assertThrowsInstanceOf(() => stack.adopt(3, (v) => values.push(v)), ReferenceError);
  assertArrayEq(values, [2, 1]);
}

{
  const values = [];
  const stack = new AsyncDisposableStack();
  assertEq(stack.adopt(1, (v) => values.push(v)), 1);
  const obj = { value: 2 };
  assertEq(stack.adopt(obj, (v) => values.push(v.value)), obj);
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
    }
  }
  testDisposalsWithAwaitUsing();
  drainJobQueue();
  assertEq(stack.disposed, true);
  assertArrayEq(values, [4, 3, 2, 1]);
}

{
  const disposed = [];
  const stack = new AsyncDisposableStack();
  stack.adopt(null, (v) => disposed.push(v));
  stack.adopt(undefined, (v) => disposed.push(v));
  stack.adopt(1, (v) => disposed.push(v));
  stack.disposeAsync();
  drainJobQueue();
  assertArrayEq(disposed, [1, undefined, null]);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.adopt(1, undefined);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.adopt(1, null);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new AsyncDisposableStack();
    stack.adopt(1, 1);
  }, TypeError);
}
