// |reftest| shell-option(--enable-regexp-escape) skip-if(!RegExp.escape||!xulRuntime.shell) -- RegExp.escape is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-regexp.escape
description: >
  RegExp.escape.name property descriptor
info: |
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [RegExp.escape]
---*/

verifyProperty(RegExp.escape, "name", {
  value: 'escape',
  writable: false,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
