// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-asyncdisposablestack-constructor
description: >
  The prototype of AsyncDisposableStack is Function.prototype
info: |
  The value of the [[Prototype]] internal slot of the AsyncDisposableStack object is the
  intrinsic object %FunctionPrototype%.
features: [explicit-resource-management]
---*/

assert.sameValue(
  Object.getPrototypeOf(AsyncDisposableStack),
  Function.prototype,
  'Object.getPrototypeOf(AsyncDisposableStack) returns the value of `Function.prototype`'
);

reportCompare(0, 0);
