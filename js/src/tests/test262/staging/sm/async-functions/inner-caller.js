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
function g() {
      return g.caller;
}

(async function f() {
  var inner = g();
  assert.sameValue(inner, null);
})();

(async function f() {
  "use strict";
  var inner = g();
  assert.sameValue(inner, null);
})();


reportCompare(0, 0);
