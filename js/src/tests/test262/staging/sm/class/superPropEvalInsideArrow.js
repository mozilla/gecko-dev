// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/
class foo {
    constructor() { }

    method() {
        return (() => eval('super.toString'));
    }
}
assert.sameValue(new foo().method()(), Object.prototype.toString);


reportCompare(0, 0);
