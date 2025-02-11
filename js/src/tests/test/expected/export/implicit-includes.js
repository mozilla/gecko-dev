// Copyright (C) 2024 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [include.js, deepEqual.js, compareArray.js]
flags: [noStrict]
description: |
  pending
esid: pending
---*/
assert.sameValue("", "");
assert.deepEqual("", "");
assert.compareArray([], []);
$262.detachArrayBuffer(new ArrayBuffer());
createNewGlobal();
