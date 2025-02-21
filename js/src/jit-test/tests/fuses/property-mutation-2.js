function test() {
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, true);

  // Redefine Array.prototype properties but don't change their values.
  // This shouldn't pop the fuse.
  let proto = Array.prototype;
  for (let i = 0; i < 3; i++) {
    for (let p of Reflect.ownKeys(proto)) {
      let desc = Object.getOwnPropertyDescriptor(proto, p);
      if (desc.configurable) {
        // Change desc.enumerable to ensure this isn't a no-op.
        desc.enumerable = !desc.enumerable;
        Object.defineProperty(proto, p, desc);  
      }
    }
  }
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, true);

  // Now actually change the value. This pops the fuse.
  let desc = Object.getOwnPropertyDescriptor(proto, Symbol.iterator);
  desc.value = function() {};
  Object.defineProperty(proto, Symbol.iterator, desc);  
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, false);
}
test();
