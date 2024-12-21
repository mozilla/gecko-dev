// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-disposablestack.prototype.disposed
description: >
  DisposableStack.prototype.disposed.name value and descriptor.
info: |
  get DisposableStack.prototype.disposed

  17 ECMAScript Standard Built-in Objects

  Functions that are specified as get or set accessor functions of built-in
  properties have "get " or "set " prepended to the property name string.

includes: [propertyHelper.js]
features: [explicit-resource-management]
---*/

var descriptor = Object.getOwnPropertyDescriptor(DisposableStack.prototype, 'disposed');

assert.sameValue(descriptor.get.name,
  'get disposed',
  'The value of `descriptor.get.name` is `get disposed`'
);

verifyProperty(descriptor.get, 'name', {
  writable: false,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
