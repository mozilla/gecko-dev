// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const stack = new DisposableStack();
  assertEq(stack.defer(() => disposed.push(1)), undefined);
  stack.defer(() => disposed.push(2));
  assertEq(stack.disposed, false);
  stack.dispose();
  assertEq(stack.disposed, true);
  assertArrayEq(disposed, [2, 1]);
  stack.dispose();
  assertArrayEq(disposed, [2, 1]);
  assertThrowsInstanceOf(() => stack.defer(() => disposed.push(3)), ReferenceError);
}

{
  const values = [];
  const stack = new DisposableStack();
  stack.defer(() => values.push(1));
  stack.defer(() => values.push(2));
  assertEq(stack.disposed, false);
  {
    using stk = stack;
    stk.use({
      [Symbol.dispose]() {
        values.push(3);
      },
    });
    stk.adopt(4, (v) => values.push(v));
    stk.defer(() => values.push(5));
  }
  assertEq(stack.disposed, true);
  assertArrayEq(values, [5, 4, 3, 2, 1]);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.defer(undefined);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.defer(null);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.defer(1);
  }, TypeError);
}
