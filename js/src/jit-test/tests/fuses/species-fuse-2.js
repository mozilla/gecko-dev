// Test for array species fuse with multiple realms.
function test() {
  var g = newGlobal();
  var arr = g.evaluate(`[1, 2, 3]`);
  var count = 0;
  Object.defineProperty(g.Array.prototype, "constructor", {get: function() {
    count++;
    return Array;
  }});
  for (var i = 0; i < 20; i++) {
    assertEq(Array.prototype.slice.call(arr).length, 3);
  }
  assertEq(count, 20);
  assertEq(getFuseState().OptimizeArraySpeciesFuse.intact, true);
  assertEq(g.getFuseState().OptimizeArraySpeciesFuse.intact, false);
}
test();
