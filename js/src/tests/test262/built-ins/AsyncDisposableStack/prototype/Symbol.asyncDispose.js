// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-asyncdisposablestack.prototype-@@asyncDispose
description: Initial state of the Symbol.asyncDispose property
info: |
  The initial value of the @@asyncDispose property is the same function object as
  the initial value of the disposeAsync property.

  Per ES6 section 17, the method should exist on the Array prototype, and it
  should be writable and configurable, but not enumerable.
includes: [propertyHelper.js]
features: [explicit-resource-management]
---*/

verifyProperty(AsyncDisposableStack.prototype, Symbol.asyncDispose, {
  value: AsyncDisposableStack.prototype.disposeAsync,
  enumerable: false,
  writable: true,
  configurable: true,
});

reportCompare(0, 0);
