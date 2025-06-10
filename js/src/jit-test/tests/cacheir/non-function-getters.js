with ({}) {}

function makeObjWithFunctionGetter(n) {
  var o = {};
  Object.defineProperty(o, "x", {
    get() { return n; }
  });

  return o;
}

function makeObjWithProxyGetter() {
  var inner = () => "proxy";
  var proxy = new Proxy(inner, {});

  var o = {};
  Object.defineProperty(o, "x", {
    get: proxy
  });
  return o;
}

function makeObjWithBoundGetter() {
  var inner = () => "bound";;
  var bound = inner.bind({});

  var o = {};
  Object.defineProperty(o, "x", {
    get: bound
  });
  return o;
}

function foo(o) { return o.x; }

for (var i = 0; i < 100; i++) {
  foo(makeObjWithFunctionGetter(i));
}

assertEq(foo(makeObjWithProxyGetter()), "proxy");
assertEq(foo(makeObjWithBoundGetter()), "bound");
