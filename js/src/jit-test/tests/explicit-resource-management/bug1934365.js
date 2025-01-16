// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

if ('oomTest' in this) {
  function b() {
    throw new Error
  }
  function c() {
    for (let i = 0; i < 100; i++) {
      d = new AsyncDisposableStack
      d.defer(() => e)
      d.defer(() => b())
      d.disposeAsync()
    }
  }

  oomTest(() => c());
}
