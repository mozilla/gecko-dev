function test() {
  var ta = new Int8Array(1);
  Object.setPrototypeOf(Array.prototype, ta);
  for (var i = 0; i < 20; i++) {
    var arr = [];
    arr[0] = 1;
    arr[10] = 1; // No-op
    arr[1_000_000] = 1; // No-op
    assertEq(arr.length, 1);
    assertEq(arr[0], 1);
  }
  assertEq(ta[0], 0);
}
test();
