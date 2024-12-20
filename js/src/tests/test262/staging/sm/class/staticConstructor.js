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
class testBasic {
    constructor() { }
    static constructor() { }
}

class testWithExtends extends null {
    constructor() { };
    static constructor() { };
}

class testOrder {
    static constructor() { };
    constructor() { };
}

class testOrderWithExtends extends null {
    static constructor() { };
    constructor() { };
}


reportCompare(0, 0);
