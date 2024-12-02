// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management") || helperThreadCount() === 0; --enable-explicit-resource-management

evalInWorker(`
  function c() {
    d = new AsyncDisposableStack
    d.defer(() => e)
    d.defer(() => c())
    d.disposeAsync()
  } c();
`)
