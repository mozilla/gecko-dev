// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-JSON-shell.js, sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/
testJSON('{"Numbers cannot have leading zeroes": 013}', true);

/******************************************************************************/

print("Tests complete");

reportCompare(0, 0);
