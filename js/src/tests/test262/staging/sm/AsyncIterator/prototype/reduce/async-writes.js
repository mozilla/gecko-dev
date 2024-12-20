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

let x = {a: () => true};

async function* gen() {
  yield x.a();
  yield x.a();
}

gen().reduce(() => {}, 0).then(
  () => assert.sameValue(true, false, 'expected error'),
  err => assert.sameValue(err instanceof Error, true),
);

x.a = () => {
  throw Error();
};


reportCompare(0, 0);
