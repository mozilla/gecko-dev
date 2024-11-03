// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  function testDisposeHandlingWhenScopeReceivesNoDisposablesSwitchCase(cs) {
    switch (cs) {
      case 'has_dispose':
        using d = {
          [Symbol.dispose]() {
            disposed.push(0);
          }
        }
        break;
      case 'no_dispose':
        break;
    }
  }
  testDisposeHandlingWhenScopeReceivesNoDisposablesSwitchCase('no_dispose');
  assertArrayEq(disposed, []);
  testDisposeHandlingWhenScopeReceivesNoDisposablesSwitchCase('has_dispose');
  assertArrayEq(disposed, [0]);
}

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
