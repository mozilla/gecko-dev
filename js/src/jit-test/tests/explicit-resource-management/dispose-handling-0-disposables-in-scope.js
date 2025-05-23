// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  function testDisposeHandlingWhenScopeReceivesNoDisposablesIf(cond) {
    if (cond) {
      return;
    }
    using x = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
  }
  testDisposeHandlingWhenScopeReceivesNoDisposablesIf(true);
  assertArrayEq(disposed, []);
  testDisposeHandlingWhenScopeReceivesNoDisposablesIf(false);
  assertArrayEq(disposed, [0]);
}
