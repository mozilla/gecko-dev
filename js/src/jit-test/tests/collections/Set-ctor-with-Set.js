load(libdir + "asserts.js");

function testOptimized1() {
  var obj = {};
  var s = new Set([obj, undefined, 3.1415]);
  for (var i = 0; i < 15; i++) {
    var clone = new Set(s);
    s.add(i); // Not added to `clone`.
    s = clone;
  }
  assertEq(s.size, 3);
  assertEq(s.has(obj), true);
  assertEq(s.has(undefined), true);
  assertEq(s.has(3.1415), true);
  assertEq(Array.from(s).toString(), "[object Object],,3.1415");
  return s;
}
testOptimized1();

function testOptimized2() {
  var s = new Set();
  for (var i = 0; i < 15; i++) {
    s = new Set(s);
    s.add(i);
  }
  assertEq(s.size, 15);
  assertEq(Array.from(s).toString(), "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14");
  return s;
}
testOptimized2();

function testOtherProto() {
  var s = new Set([1, 2, 3]);
  Object.setPrototypeOf(s, null);
  for (var i = 0; i < 15; i++) {
    assertThrowsInstanceOf(() => new Set(s), TypeError);
  }
}
testOtherProto();

function testOwnIteratorProp() {
  var s = new Set([1, 2, 3]);
  var c = 0;
  s[Symbol.iterator] = function() {
    c++;
    return {next() { return {done: true}; }};
  };
  for (var i = 0; i < 15; i++) {
    assertEq(new Set(s).size, 0);
  }
  assertEq(c, 15);
}
testOwnIteratorProp();

function testCustomProtoIteratorProp() {
  // Use a new global because this pops a realm fuse.
  newGlobal().evaluate(`
    var s = new Set([1, 2, 3]);
    var c = 0;
    assertEq(getFuseState().OptimizeSetObjectIteratorFuse.intact, true);
    Set.prototype[Symbol.iterator] = function() {
      c++;
      return {next() { return {done: true}; }};
    };
    assertEq(getFuseState().OptimizeSetObjectIteratorFuse.intact, false);
    for (var i = 0; i < 15; i++) {
      assertEq(new Set(s).size, 0);
    }
    assertEq(c, 15);
  `);
}
testCustomProtoIteratorProp();

function testCustomProtoIteratorPropEmpty() {
  newGlobal().evaluate(`
    var s = new Set();
    var c = 0;
    assertEq(getFuseState().OptimizeSetObjectIteratorFuse.intact, true);
    Set.prototype[Symbol.iterator] = function() {
      c++;
      return {next() { return {done: true}; }};
    };
    assertEq(getFuseState().OptimizeSetObjectIteratorFuse.intact, false);
    for (var i = 0; i < 15; i++) {
      assertEq(new Set(s).size, 0);
    }
    assertEq(c, 15);
  `);
}
testCustomProtoIteratorPropEmpty();

function testCustomIteratorNext() {
  newGlobal().evaluate(`
    var iterProto = Object.getPrototypeOf(new Set()[Symbol.iterator]());
    var s = new Set([1, 2, 3]);
    var c = 0;
    assertEq(getFuseState().OptimizeSetObjectIteratorFuse.intact, true);
    iterProto.next = function() {
      c++;
      return {done: true};
    };
    assertEq(getFuseState().OptimizeSetObjectIteratorFuse.intact, false);
    for (var i = 0; i < 15; i++) {
      assertEq(new Set(s).size, 0);
    }
    assertEq(c, 15);
  `);
}
testCustomIteratorNext();
