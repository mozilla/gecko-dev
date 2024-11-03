// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  assertEq(typeof DisposableStack, "function");

  assertDeepEq(Object.getOwnPropertyDescriptor(DisposableStack, 'prototype'), {
    value: DisposableStack.prototype,
    writable: false,
    enumerable: false,
    configurable: false,
  });
}

{
  assertDeepEq(Object.getOwnPropertyDescriptor(DisposableStack.prototype, Symbol.toStringTag), {
    value: 'DisposableStack',
    writable: false,
    enumerable: false,
    configurable: true
  });
}

{
  assertEq(typeof DisposableStack.prototype[Symbol.dispose], 'function');
  assertEq(DisposableStack.prototype[Symbol.dispose], DisposableStack.prototype.dispose);
  assertDeepEq(Object.getOwnPropertyDescriptor(DisposableStack.prototype, Symbol.dispose), {
    value: DisposableStack.prototype[Symbol.dispose],
    writable: true,
    enumerable: false,
    configurable: true,
  });
}

{
  assertThrowsInstanceOf(() => DisposableStack(), TypeError);
}

{
  const properties = ['dispose', 'adopt', 'defer', 'move', 'disposed', 'use', Symbol.dispose];
  for (const p of properties) {
    assertThrowsInstanceOf(() => {
      DisposableStack.prototype[p].call({});
    }, TypeError);
  }
}
