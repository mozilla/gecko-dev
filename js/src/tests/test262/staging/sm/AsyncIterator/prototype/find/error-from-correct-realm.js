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
assert.sameValue(TypeError !== otherGlobal.TypeError, true);

async function *gen() {}

gen().find().then(() => assert.sameValue(true, false, 'expected error'), err => {
  assert.sameValue(err instanceof TypeError, true);
});

otherGlobal.AsyncIterator.prototype.find.call(gen()).then(() => assert.sameValue(true, false, 'expected error'), err => {
  assert.sameValue(
    err instanceof otherGlobal.TypeError,
    true,
    'TypeError comes from the realm of the method.',
  );
});


reportCompare(0, 0);
