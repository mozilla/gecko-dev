// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

let called = 0;

{
  using d = {
    get [Symbol.dispose]() {
      called++;
      return () => {};
    }
  };
}

assertEq(called, 1);
