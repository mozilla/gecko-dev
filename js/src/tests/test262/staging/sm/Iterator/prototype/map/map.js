// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %Iterator.prototype%.map value and descriptor.
info: |
  17 ECMAScript Standard Built-in Objects
features:
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

const map = Reflect.getOwnPropertyDescriptor(Iterator.prototype, 'map');

assert.sameValue(
  Iterator.prototype.map, map.value,
  'The value of `%Iterator.prototype%.map` is the same as the value in the property descriptor.'
);

assert.sameValue(
  typeof map.value, 'function',
  '%Iterator.prototype%.map is a function.'
);

assert.sameValue(map.enumerable, false);
assert.sameValue(map.writable, true);
assert.sameValue(map.configurable, true);


reportCompare(0, 0);
