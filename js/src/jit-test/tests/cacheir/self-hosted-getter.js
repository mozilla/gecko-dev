// |jit-test| --fast-warmup; --no-threads

function makeObjWithSelfHostedGetter() {
  var o = {};
  Object.defineProperty(o, "x", {
    get: Number.isFinite
  });
  return o;
}

for (var i = 0; i < 100; i++) {
  let o = makeObjWithSelfHostedGetter();
  o.x;
}
