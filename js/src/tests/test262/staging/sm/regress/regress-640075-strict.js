'use strict';
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
flags:
- onlyStrict
includes: [sm/non262-shell.js, sm/non262.js]
description: |
  pending
esid: pending
---*/
"use strict";
assertThrowsInstanceOf(
    () => eval("(function() { eval(); function eval() {} })"),
    SyntaxError
)


reportCompare(0, 0);
