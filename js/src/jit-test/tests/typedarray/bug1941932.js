let o = new Uint8Array(10);
o.prop = 0;

function has(k) { return Object.hasOwn(o, k); }

for (var i = 0; i < 100; i++) {
  assertEq(has("prop"), true);
  assertEq(has("a"), false);
  assertEq(has("b"), false);
  assertEq(has("c"), false);
  assertEq(has("d"), false);
  assertEq(has("e"), false);
  assertEq(has("f"), false);
}

assertEq(has("5"), true);
assertEq(has("11"), false);
