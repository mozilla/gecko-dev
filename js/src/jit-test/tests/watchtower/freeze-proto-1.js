// |jit-test| --fast-warmup; --no-threads

// Freezing a prototype object must invalidate the megamorphic set-property cache.

var atoms = ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"];

function addProps(obj) {
  for (var i = 0; i < 10; i++) {
    obj[atoms[i]] = i;
  }
}

function f() {
  var proto = Object.create(null);
  proto.c = 0;
  proto.g = 0;
  proto.i = 0;

  var obj1 = Object.create(proto);
  var obj2 = Object.create(proto);

  addProps(obj1);
  Object.freeze(proto);
  addProps(obj2);

  assertEq(Object.keys(obj1).length, 10);
  assertEq(Object.keys(obj2).length, 7);
}
for (var i = 0; i < 10; i++) {
  f();
}
