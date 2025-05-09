// Test for shared array buffer species fuse with multiple realms.
function test() {
  var g = newGlobal();
  var arr = g.evaluate(`new SharedArrayBuffer(4)`);
  var count = 0;
  Object.defineProperty(g.SharedArrayBuffer.prototype, "constructor", {get: function() {
    count++;
    return SharedArrayBuffer;
  }});
  for (var i = 0; i < 20; i++) {
    assertEq(SharedArrayBuffer.prototype.slice.call(arr).byteLength, 4);
  }
  assertEq(count, 20);
  assertEq(getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, true);
  assertEq(g.getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, false);
}
test();
