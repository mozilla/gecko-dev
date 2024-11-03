// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The prototype of AsyncDisposableStack.prototype is Object.prototype
esid: sec-properties-of-the-asyncdisposablestack-prototype-object
info: |
  The value of the [[Prototype]] internal slot of the AsyncDisposableStack prototype object
  is the intrinsic object %Object.prototype%.
features: [explicit-resource-management]
---*/

var proto = Object.getPrototypeOf(AsyncDisposableStack.prototype);
assert.sameValue(proto, Object.prototype);

reportCompare(0, 0);
