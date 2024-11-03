// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const disposed = [];

foo: {
  using d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  };
  {
    let a = 0, b = () => a;
    break foo;
  }
}
assertArrayEq(disposed, [1]);
