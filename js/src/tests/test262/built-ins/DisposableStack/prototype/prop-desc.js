// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The property descriptor DisposableStack.prototype
esid: sec-properties-of-the-disposablestack-constructor
info: |
  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false,
  [[Configurable]]: false }.
features: [explicit-resource-management]
includes: [propertyHelper.js]
---*/

verifyProperty(DisposableStack, 'prototype', {
  writable: false,
  enumerable: false,
  configurable: false
});

reportCompare(0, 0);
