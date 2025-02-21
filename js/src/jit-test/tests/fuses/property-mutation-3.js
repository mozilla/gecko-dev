function test() {
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, true);

  // This defineProperty doesn't change the value so the fuse stays intact.
  let proto = Array.prototype;
  let desc = Object.getOwnPropertyDescriptor(proto, Symbol.iterator);
  Object.defineProperty(proto, Symbol.iterator, desc);  
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, true);

  // Changing the property to an accessor property must pop the fuse.
  desc.get = desc.value;
  delete desc.value;
  delete desc.writable;
  Object.defineProperty(proto, Symbol.iterator, desc);
  assertEq(getFuseState().ArrayPrototypeIteratorFuse.intact, false);
}
test();
