// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Smoketest of %Iterator.prototype%.take.
info: |
  Iterator Helpers proposal 2.1.5.4
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

let iter = [1, 2, 3].values().take(2);

for (const v of [1, 2]) {
  let result = iter.next();
  assert.sameValue(result.done, false);
  assert.sameValue(result.value, v);
}

assert.sameValue(iter.next().done, true);

// `take`, when called without arguments, throws a RangeError,
assertThrowsInstanceOf(() => ['test'].values().take(), RangeError);


reportCompare(0, 0);
