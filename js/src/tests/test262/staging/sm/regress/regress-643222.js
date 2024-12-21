/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/
/* This shouldn't trigger an assertion. */
(function () {
    eval("var x=delete(x)")
})();


reportCompare(0, 0);
