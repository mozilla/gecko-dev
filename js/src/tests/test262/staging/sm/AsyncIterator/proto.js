// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
  The prototype of the AsyncIterator constructor is the intrinsic object %FunctionPrototype%.

  AsyncIterator is not enabled unconditionally
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- async-iteration
description: |
  pending
esid: pending
---*/
assert.sameValue(Object.getPrototypeOf(AsyncIterator), Function.prototype);

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator, 'prototype');
assert.sameValue(propDesc.writable, false);
assert.sameValue(propDesc.enumerable, false);
assert.sameValue(propDesc.configurable, false);


reportCompare(0, 0);
