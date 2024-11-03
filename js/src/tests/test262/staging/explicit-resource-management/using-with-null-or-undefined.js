// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: using with null or undefined should not throw.
includes: [compareArray.js]
features: [explicit-resource-management]
---*/

// Use using with null --------------
let withNullvalues = [];

(function TestUsingWithNull() {
  let using = null;
  withNullvalues.push(42);
})();
assert.compareArray(withNullvalues, [42]);

// Use using with undefined --------------
let withUndefinedvalues = [];

(function TestUsingWithUndefined() {
  let using = undefined;
  withUndefinedvalues.push(42);
})();
assert.compareArray(withUndefinedvalues, [42]);

reportCompare(0, 0);
