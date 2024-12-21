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
// Make sure it doesn't matter when we make the arrow function
new class extends class { } {
    constructor() {
        let arrow = () => this;
        assertThrowsInstanceOf(arrow, ReferenceError);
        super();
        assert.sameValue(arrow(), this);
    }
}();


reportCompare(0, 0);
