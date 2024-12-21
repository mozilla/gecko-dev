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
var array = [1, 2, 3];

var calls = 0;

var grouped = Object.groupBy(array, () => {
  calls++;

  return {
    toString() {
      return "a";
    }
  }
});

assert.sameValue(calls, 3);


reportCompare(0, 0);
