// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  assertEq(typeof AsyncDisposableStack, "function");

  assertDeepEq(Object.getOwnPropertyDescriptor(AsyncDisposableStack, 'prototype'), {
    value: AsyncDisposableStack.prototype,
    writable: false,
    enumerable: false,
    configurable: false,
  });
}

{
  assertDeepEq(Object.getOwnPropertyDescriptor(AsyncDisposableStack.prototype, Symbol.toStringTag), {
    value: 'AsyncDisposableStack',
    writable: false,
    enumerable: false,
    configurable: true
  });
}

{
  assertEq(typeof AsyncDisposableStack.prototype[Symbol.asyncDispose], 'function');
  assertEq(AsyncDisposableStack.prototype[Symbol.asyncDispose], AsyncDisposableStack.prototype.disposeAsync);
  assertDeepEq(Object.getOwnPropertyDescriptor(AsyncDisposableStack.prototype, Symbol.asyncDispose), {
    value: AsyncDisposableStack.prototype[Symbol.asyncDispose],
    writable: true,
    enumerable: false,
    configurable: true,
  });
}

{
  assertThrowsInstanceOf(() => AsyncDisposableStack(), TypeError);
}

{
  const properties = ['adopt', 'defer', 'move', 'disposed', 'use'];
  for (const p of properties) {
    assertThrowsInstanceOf(() => {
      AsyncDisposableStack.prototype[p].call({});
    }, TypeError);
  }

  const asyncProperties = ['disposeAsync', Symbol.asyncDispose];
  for (const p of asyncProperties) {
    assertThrowsInstanceOfAsync(async () => {
      await AsyncDisposableStack.prototype[p].call({});
    }, TypeError);
  }
}
