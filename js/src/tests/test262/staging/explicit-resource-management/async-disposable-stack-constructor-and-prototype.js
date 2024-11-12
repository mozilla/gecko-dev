// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test AsyncDisposableStack constructor and prototype.
includes: [propertyHelper.js]
features: [globalThis, explicit-resource-management]
---*/

// constructor --------
assert.sameValue(
    typeof AsyncDisposableStack, 'function',
    'The value of `typeof AsyncDisposableStack` is "function"');

// prototype --------
verifyProperty(AsyncDisposableStack, 'prototype', {
  value: AsyncDisposableStack.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

reportCompare(0, 0);
