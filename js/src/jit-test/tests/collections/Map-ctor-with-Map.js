load(libdir + "asserts.js");

function testOptimized1() {
  var obj = {};
  var m = new Map([[obj, undefined], [undefined, obj], [3.1415, 123]]);
  for (var i = 0; i < 15; i++) {
    var clone = new Map(m);
    m.set(i, i); // Not added to `clone`.
    m = clone;
  }
  assertEq(m.size, 3);
  assertEq(m.get(obj), undefined);
  assertEq(m.get(undefined), obj);
  assertEq(m.get(3.1415), 123);
  assertEq(Array.from(m).toString(), "[object Object],,,[object Object],3.1415,123");
  return m;
}
testOptimized1();

function testOptimized2() {
  var m = new Map();
  for (var i = 0; i < 15; i++) {
    m = new Map(m);
    m.set(i, i);
  }
  assertEq(m.size, 15);
  assertEq(Array.from(m.keys()).toString(), "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14");
  return m;
}
testOptimized2();

function testOtherProto() {
  var m = new Map([[1, 1], [2, 2]]);
  Object.setPrototypeOf(m, null);
  for (var i = 0; i < 15; i++) {
    assertThrowsInstanceOf(() => new Map(m), TypeError);
  }
}
testOtherProto();

function testOwnIteratorProp() {
  var m = new Map([[1, 1], [2, 2]]);
  var c = 0;
  m[Symbol.iterator] = function() {
    c++;
    return {next() { return {done: true}; }};
  };
  for (var i = 0; i < 15; i++) {
    assertEq(new Map(m).size, 0);
  }
  assertEq(c, 15);
}
testOwnIteratorProp();

function testCustomProtoIteratorProp() {
  // Use a new global because this pops a realm fuse.
  newGlobal().evaluate(`
    var m = new Map([[1, 1], [2, 2]]);
    var c = 0;
    assertEq(getFuseState().OptimizeMapObjectIteratorFuse.intact, true);
    Map.prototype[Symbol.iterator] = function() {
      c++;
      return {next() { return {done: true}; }};
    };
    assertEq(getFuseState().OptimizeMapObjectIteratorFuse.intact, false);
    for (var i = 0; i < 15; i++) {
      assertEq(new Map(m).size, 0);
    }
    assertEq(c, 15);
  `);
}
testCustomProtoIteratorProp();

function testCustomProtoIteratorPropEmpty() {
  newGlobal().evaluate(`
    var m = new Map();
    var c = 0;
    assertEq(getFuseState().OptimizeMapObjectIteratorFuse.intact, true);
    Map.prototype[Symbol.iterator] = function() {
      c++;
      return {next() { return {done: true}; }};
    };
    assertEq(getFuseState().OptimizeMapObjectIteratorFuse.intact, false);
    for (var i = 0; i < 15; i++) {
      assertEq(new Map(m).size, 0);
    }
    assertEq(c, 15);
  `);
}
testCustomProtoIteratorPropEmpty();

function testCustomIteratorNext() {
  newGlobal().evaluate(`
    var iterProto = Object.getPrototypeOf(new Map()[Symbol.iterator]());
    var m = new Map([[1, 1], [2, 2]]);
    var c = 0;
    assertEq(getFuseState().OptimizeMapObjectIteratorFuse.intact, true);
    iterProto.next = function() {
      c++;
      return {done: true};
    };
    assertEq(getFuseState().OptimizeMapObjectIteratorFuse.intact, false);
    for (var i = 0; i < 15; i++) {
      assertEq(new Map(m).size, 0);
    }
    assertEq(c, 15);
  `);
}
testCustomIteratorNext();
