// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  `name` property of AsyncIterator.prototype.find.
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- async-iteration
esid: pending
---*/
const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.find, 'name');
assert.sameValue(propDesc.value, 'find');
assert.sameValue(propDesc.writable, false);
assert.sameValue(propDesc.enumerable, false);
assert.sameValue(propDesc.configurable, true);


reportCompare(0, 0);
