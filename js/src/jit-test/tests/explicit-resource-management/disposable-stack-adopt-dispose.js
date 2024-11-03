// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const adopted = [];
  const stack = new DisposableStack();
  assertEq(stack.adopt(1, (v) => adopted.push(v)), 1);
  const obj = { value: 2 };
  assertEq(stack.adopt(obj, (v) => adopted.push(v.value)), obj);
  assertEq(stack.disposed, false);
  stack.dispose();
  assertEq(stack.disposed, true);
  assertArrayEq(adopted, [2, 1]);
  stack.dispose();
  assertArrayEq(adopted, [2, 1]);
  assertThrowsInstanceOf(() => stack.adopt(3, (v) => adopted.push(v)), ReferenceError);
}

{
  const values = [];
  const stack = new DisposableStack();
  assertEq(stack.adopt(1, (v) => values.push(v)), 1);
  const obj = { value: 2 };
  assertEq(stack.adopt(obj, (v) => values.push(v.value)), obj);
  assertEq(stack.disposed, false);
  {
    using stk = stack;
    stk.use({
      [Symbol.dispose]() {
        values.push(3);
      },
    });
    stk.adopt(4, (v) => values.push(v));
  }
  assertEq(stack.disposed, true);
  assertArrayEq(values, [4, 3, 2, 1]);
}

{
  const disposed = [];
  const stack = new DisposableStack();
  stack.adopt(null, (v) => disposed.push(v));
  stack.adopt(undefined, (v) => disposed.push(v));
  stack.adopt(1, (v) => disposed.push(v));
  stack.dispose();
  assertArrayEq(disposed, [1, undefined, null]);
}

{
  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.adopt(1, undefined);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.adopt(1, null);
  }, TypeError);

  assertThrowsInstanceOf(() => {
    const stack = new DisposableStack();
    stack.adopt(1, 1);
  }, TypeError);
}
