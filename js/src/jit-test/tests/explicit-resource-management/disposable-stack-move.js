// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const stack = new DisposableStack();
  stack.use({
    [Symbol.dispose]() {
      disposed.push(1);
    },
  });
  stack.adopt(2, (v) => disposed.push(v));
  stack.defer(() => disposed.push(3));
  assertEq(stack.disposed, false);
  const newStack = stack.move();
  assertEq(stack.disposed, true);
  assertThrowsInstanceOf(() => stack.use({
    [Symbol.dispose]() {
      disposed.push(4);
    },
  }), ReferenceError);
  assertEq(newStack.disposed, false);
  stack.dispose();
  assertArrayEq(disposed, []);
  assertEq(newStack.disposed, false);
  newStack.dispose();
  assertArrayEq(disposed, [3, 2, 1]);
  assertEq(newStack.disposed, true);
}

{
  const disposed = [];
  function createScopeSharedResources() {
    const stack = new DisposableStack();
    stack.use({
      [Symbol.dispose]() {
        disposed.push(1);
      },
    });
    stack.adopt(2, (v) => disposed.push(v));
    stack.defer(() => disposed.push(3));
    return () => {
      using stk = stack.move();
      assertEq(stack.disposed, true);
      assertEq(stk.disposed, false);
      stk.adopt(4, (v) => disposed.push(v));
    }
  }
  const disposeScopeSharedResources = createScopeSharedResources();
  assertArrayEq(disposed, []);
  disposeScopeSharedResources();
  assertArrayEq(disposed, [4, 3, 2, 1]);
}

{
  const stk = new DisposableStack();
  stk.defer(() => {});
  const newStack = stk.move();
  assertEq(newStack === stk, false);
}

{
  const disposed = [];
  assertThrowsInstanceOf(() => {
    const stk = new DisposableStack();
    stk.defer(() => disposed.push(1));
    stk.dispose();
    const newStack = stk.move();
  }, ReferenceError);
}
