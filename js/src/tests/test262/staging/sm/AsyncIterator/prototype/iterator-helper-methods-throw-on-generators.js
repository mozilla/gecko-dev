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

async function *gen() { yield 'value'; }

const asyncIteratorHelperProto = Object.getPrototypeOf(gen().map(x => x));

assertThrowsInstanceOf(() => asyncIteratorHelperProto.next.call(gen()), TypeError);
assertThrowsInstanceOf(() => asyncIteratorHelperProto.return.call(gen()), TypeError);
assertThrowsInstanceOf(() => asyncIteratorHelperProto.throw.call(gen()), TypeError);


reportCompare(0, 0);
