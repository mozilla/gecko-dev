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

assert.sameValue([1, 2, 3].values().reduce((x, y) => `(${x}+${y})`, 0), '(((0+1)+2)+3)');
assert.sameValue([1, 2, 3].values().reduce((x, y) => `(${x}+${y})`), '((1+2)+3)');


reportCompare(0, 0);
