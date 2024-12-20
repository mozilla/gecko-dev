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
function test() {
    delete arguments[1];
    return Array.prototype.join.call(arguments);
}
assert.sameValue(test(1,2,3), "1,,3");
Object.prototype[1] = "ponies!!!1";
assert.sameValue(test(1,2,3), "1,ponies!!!1,3");

reportCompare(0, 0);
