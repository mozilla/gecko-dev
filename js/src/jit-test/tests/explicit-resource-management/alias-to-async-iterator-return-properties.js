// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

async function* generator() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(generator.prototype));

assertEq(typeof AsyncIteratorPrototype[Symbol.asyncDispose], 'function');
assertDeepEq(Object.getOwnPropertyDescriptor(AsyncIteratorPrototype[Symbol.asyncDispose], 'length'), {
  value: 0,
  writable: false,
  enumerable: false,
  configurable: true,
});
assertDeepEq(Object.getOwnPropertyDescriptor(AsyncIteratorPrototype[Symbol.asyncDispose], 'name'), {
  value: '[Symbol.asyncDispose]',
  writable: false,
  enumerable: false,
  configurable: true,
});
assertDeepEq(Object.getOwnPropertyDescriptor(AsyncIteratorPrototype, Symbol.asyncDispose), {
  value: AsyncIteratorPrototype[Symbol.asyncDispose],
  writable: true,
  enumerable: false,
  configurable: true,
});
assertEq(AsyncIteratorPrototype[Symbol.asyncDispose]() instanceof Promise, true);
