function testNullUndefined() {
  for (var i = 0; i < 100; i++) {
    var v = (i & 1) ? null : undefined;

    var map = new Map(v);
    assertEq(map.size, 0);

    var set = new Set(v);
    assertEq(set.size, 0);
  }
}
testNullUndefined();

function testProtoGetter() {
  var count = 0;
  Object.defineProperty(Array.prototype, 1, { get: () => count++ });
  for (var i = 0; i < 100; i++) {
    var map = new Map([
      [1, 1],
      [2, 2],
      [3, /* hole */, 3],
    ]);
    assertEq(map.size, 3);
    assertEq(map.get(3), i * 3);

    var set = new Set(["a", /* hole */, "c"]);
    assertEq(set.size, 3);
    assertEq(set.has(i * 3 + 1), true);

    new Set([1, /* hole */,]); // Side-effects! Must not be optimized away.
  }
  assertEq(count, 300);
}
testProtoGetter();

// The |iterable| argument can be a primitive value.
function testPrimitiveIterable() {
  Number.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];
  Number.prototype.length = 2;
  Number.prototype[0] = ["a", "b"];
  Number.prototype[1] = ["c", "d"];

  for (var i = 0; i < 100; i++) {
    var map = new Map(i);
    assertEq(map.size, 2);
    assertEq(map.get("c"), "d");

    var set = new Set(i);
    assertEq(set.size, 2);
    assertEq(set.has(Number.prototype[1]), true);

    var set2 = new Set("abc123abc");
    assertEq(set2.size, 6);
  }
}
testPrimitiveIterable();
