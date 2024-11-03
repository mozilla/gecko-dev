// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --disable-explicit-resource-management

assertEq(globalThis.DisposableStack, undefined);
assertEq(globalThis.AsyncDisposableStack, undefined);
assertEq(globalThis.SuppressedError, undefined);
