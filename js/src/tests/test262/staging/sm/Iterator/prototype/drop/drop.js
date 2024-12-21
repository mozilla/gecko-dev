// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Smoketest of %Iterator.prototype%.drop.
info: |
  Iterator Helpers proposal 2.1.5.5
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

let iter = [1, 2, 3].values().drop(1);

for (const v of [2, 3]) {
  let result = iter.next();
  assert.sameValue(result.done, false);
  assert.sameValue(result.value, v);
}

assert.sameValue(iter.next().done, true);

// `drop`, when called without arguments, throws a RangeError,
assertThrowsInstanceOf(() => ['test'].values().drop(), RangeError);


reportCompare(0, 0);
