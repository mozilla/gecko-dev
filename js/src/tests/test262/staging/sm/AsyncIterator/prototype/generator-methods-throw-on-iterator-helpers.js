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

const asyncGeneratorProto = Object.getPrototypeOf(
  Object.getPrototypeOf(
    (async function *() {})()
  )
);

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(x => (async function*() {})()),
];

for (const method of methods) {
  const iteratorHelper = method((async function*() {})());
  asyncGeneratorProto.next.call(iteratorHelper).then(
    _ => assert.sameValue(true, false, 'Expected reject'),
    err => assert.sameValue(err instanceof TypeError, true),
  );
  asyncGeneratorProto.return.call(iteratorHelper).then(
    _ => assert.sameValue(true, false, 'Expected reject'),
    err => assert.sameValue(err instanceof TypeError, true),
  );
  asyncGeneratorProto.throw.call(iteratorHelper).then(
    _ => assert.sameValue(true, false, 'Expected reject'),
    err => assert.sameValue(err instanceof TypeError, true),
  );
}


reportCompare(0, 0);
