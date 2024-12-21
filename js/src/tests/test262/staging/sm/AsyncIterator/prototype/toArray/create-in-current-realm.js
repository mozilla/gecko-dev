// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- async-iteration
description: |
  pending
esid: pending
---*/

const otherGlobal = createNewGlobal({newCompartment: true});

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

gen().toArray().then(array => {
  assert.sameValue(array instanceof Array, true);
  assert.sameValue(array instanceof otherGlobal.Array, false);
});

otherGlobal.AsyncIterator.prototype.toArray.call(gen()).then(array => {
  assert.sameValue(array instanceof Array, false);
  assert.sameValue(array instanceof otherGlobal.Array, true);
});


reportCompare(0, 0);
