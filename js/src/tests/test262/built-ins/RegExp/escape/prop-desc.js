// |reftest| shell-option(--enable-regexp-escape) skip-if(!RegExp.escape||!xulRuntime.shell) -- RegExp.escape is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The property descriptor RegExp.escape
esid: sec-regexp.escape
info: |
  17 ECMAScript Standard Built-in Objects
features: [RegExp.escape]
includes: [propertyHelper.js]
---*/

verifyProperty(RegExp, "escape", {
  writable: true,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
