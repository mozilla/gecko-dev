// Copyright (C) 2024 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [compareArray.js, deepEqual.js, include.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/
assert.sameValue("", "");
assert.deepEqual("", "");
assert.compareArray([], []);
$262.detachArrayBuffer(new ArrayBuffer());
createNewGlobal();
