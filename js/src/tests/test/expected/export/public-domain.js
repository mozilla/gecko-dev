/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
flags:
- noStrict
description: |
  pending
esid: pending
---*/
async function f() {
    let
    await 0;
}

assert.sameValue(true, f instanceof Function);
