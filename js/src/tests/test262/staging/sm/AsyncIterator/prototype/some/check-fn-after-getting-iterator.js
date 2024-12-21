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
const log = [];
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args[1]?.toString()}`);
    return Reflect[key](...args);
  },
});

class TestIterator extends AsyncIterator {
  next() {
    return Promise.resolve({done: true});
  }
}

async function* gen() {
  yield 1;
}

const iter = new Proxy(new TestIterator(), handlerProxy);
iter.some(1).then(() => assert.sameValue(true, false, 'expected error'), err => assert.sameValue(err instanceof TypeError, true));

assert.sameValue(
  log.join('\n'),
  `get: some
get: next`
);



reportCompare(0, 0);
