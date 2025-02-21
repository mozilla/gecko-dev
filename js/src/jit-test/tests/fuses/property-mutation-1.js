function test() {
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, true);

  // Re-assign Array.prototype properties but don't change the values.
  // This shouldn't pop the fuse.
  let proto = Array.prototype;
  for (let i = 0; i < 3; i++) {
    proto[Symbol.iterator] = proto[Symbol.iterator];
    for (let p of Reflect.ownKeys(proto)) {
      let v = proto[p];
      proto[p] = v;
    }
  }
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, true);

  // Now actually change the value. This pops the fuse.
  proto[Symbol.iterator] = proto.push;
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, false);
}
test();
