// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const values = [];
function functionWithParameterExpressions(param = 0) {
  using d = {
    [Symbol.dispose]() {
      values.push(1);
    }
  };
}
functionWithParameterExpressions();
assertArrayEq(values, [1]);
