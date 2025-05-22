// Copyright (C) 2016 The V8 Project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-isnan-number
description: >
  Property descriptor for isNaN
includes: [propertyHelper.js]
---*/

verifyPrimordialCallableProperty(this, "isNaN", "isNaN", 1, {
  writable: true,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
