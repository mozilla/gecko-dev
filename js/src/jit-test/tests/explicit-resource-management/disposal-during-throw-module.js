// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

globalThis.disposed = false;

const m = parseModule(`
  using x = {
    [Symbol.dispose]() {
      globalThis.disposed = true;
    }
  }
  throw new Error("err");
`);

moduleLink(m);
moduleEvaluate(m).catch(() => 0);

assertEq(globalThis.disposed, true);
