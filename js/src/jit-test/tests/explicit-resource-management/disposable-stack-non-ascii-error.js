// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

{
  try {
    const d = new DisposableStack();
    d.adopt(1, {"\u3042": 10});
  } catch (err) {
    assertEq(err instanceof TypeError, true);
    assertEq(err.message.includes("\u3042"), true);
  }
}

{
  try {
    const d = new DisposableStack();
    d.defer({"\u3042": 10});
  } catch (err) {
    assertEq(err instanceof TypeError, true);
    assertEq(err.message.includes("\u3042"), true);
  }
}
