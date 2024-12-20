// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- iterator-helpers
info: |
  Iterator is not enabled unconditionally
description: |
  pending
esid: pending
---*/

const otherGlobal = createNewGlobal({newCompartment: true});

let array = [1, 2, 3].values().toArray();
assert.sameValue(array instanceof Array, true);
assert.sameValue(array instanceof otherGlobal.Array, false);

array = otherGlobal.Iterator.prototype.toArray.call([1, 2, 3].values());
assert.sameValue(array instanceof Array, false);
assert.sameValue(array instanceof otherGlobal.Array, true);


reportCompare(0, 0);
