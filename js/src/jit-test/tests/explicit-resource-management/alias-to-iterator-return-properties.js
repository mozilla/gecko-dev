// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const IteratorPrototype = Object.getPrototypeOf(
    Object.getPrototypeOf([][Symbol.iterator]())
  );

  assertEq(typeof IteratorPrototype[Symbol.dispose], 'function');
  assertDeepEq(Object.getOwnPropertyDescriptor(IteratorPrototype[Symbol.dispose], 'length'), {
    value: 0,
    writable: false,
    enumerable: false,
    configurable: true,
  });
  assertDeepEq(Object.getOwnPropertyDescriptor(IteratorPrototype[Symbol.dispose], 'name'), {
    value: '[Symbol.dispose]',
    writable: false,
    enumerable: false,
    configurable: true,
  });
  assertDeepEq(Object.getOwnPropertyDescriptor(IteratorPrototype, Symbol.dispose), {
    value: IteratorPrototype[Symbol.dispose],
    writable: true,
    enumerable: false,
    configurable: true,
  });
}
