// Test for array buffer species fuse with multiple realms.
function test() {
  var g = newGlobal();
  var arr = g.evaluate(`new ArrayBuffer(4)`);
  var count = 0;
  Object.defineProperty(g.ArrayBuffer.prototype, "constructor", {get: function() {
    count++;
    return ArrayBuffer;
  }});
  for (var i = 0; i < 20; i++) {
    assertEq(ArrayBuffer.prototype.slice.call(arr).byteLength, 4);
  }
  assertEq(count, 20);
  assertEq(getFuseState().OptimizeArrayBufferSpeciesFuse.intact, true);
  assertEq(g.getFuseState().OptimizeArrayBufferSpeciesFuse.intact, false);
}
test();
