// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-suppressederror.prototype.constructor
description: >
  The `SuppressedError.prototype.constructor` property descriptor.
info: |
  The initial value of SuppressedError.prototype.constructor is the intrinsic
  object %SuppressedError%.

  17 ECMAScript Standard Built-in Objects:

  Every other data property described (...) has the attributes { [[Writable]]: true,
    [[Enumerable]]: false, [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [explicit-resource-management]
---*/

verifyProperty(SuppressedError.prototype, 'constructor', {
  value: SuppressedError,
  enumerable: false,
  writable: true,
  configurable: true
});

reportCompare(0, 0);
