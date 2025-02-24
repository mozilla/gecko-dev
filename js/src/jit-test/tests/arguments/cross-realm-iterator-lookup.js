// Test for `arguments[Symbol.iterator]` lookups on a cross-realm ArgumentsObject.
// The result must have the same realm as the object.
function test() {
  var g = newGlobal({sameCompartmentAs: this});
  for (var i = 0; i < 30; i++) {
    var myRealmArgs = (function() { return arguments; })(1, 2, 3);
    var crossRealmArgs = g.evaluate(`(function() { return arguments; })(1, 2, 3)`);
    var args = (i & 1) ? myRealmArgs : crossRealmArgs;
    var createIter = args[Symbol.iterator];
    assertEq(objectGlobal(createIter), (i & 1) ? this : g);
    var iter = createIter.call(args);
    assertEq(objectGlobal(iter), (i & 1) ? this : g);
    assertEq(iter.next().value, 1);
  }
}
test();
