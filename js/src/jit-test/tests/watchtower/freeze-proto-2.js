// |jit-test| --fast-warmup; --no-threads

// Freezing a prototype object must invalidate the megamorphic set-property cache.

function addProps(obj) {
  for (var i = 0; i < 10; i++) {
    obj["random$name" + i] = i;
  }
}

function f() {
  var proto = Object.create(null);
  proto.random$name0 = 0;
  proto.random$name5 = 0;
  proto.random$name9 = 0;

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
