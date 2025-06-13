// |jit-test| skip-if: !('scriptAddressForFunction' in this)

with ({}) {}

function makeObjWithFunctionGetter(n) {
  var o = {};
  Object.defineProperty(o, "x", {
    get() { return n; }
  });

  return o;
}

function makeObjWithBoundGetter() {
  // Use a testing function to leak the address of the script
  // so that we can circumvent the GuardFunctionScript.
  let orig = makeObjWithFunctionGetter(0);
  let getter = Object.getOwnPropertyDescriptor(orig, "x").get;
  let getterAddress = scriptAddressForFunction(getter);

  var inner = () => "bound";
  var bound = inner.bind(getterAddress);

  let o = {};
  Object.defineProperty(o, "x", {
    get: bound
  });
  return o;
}

function foo(o) { return o.x; }

for (var i = 0; i < 100; i++) {
  foo(makeObjWithFunctionGetter(i));
}

assertEq(foo(makeObjWithBoundGetter()), "bound");
