// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The prototype of SuppressedError constructor is Error
esid: sec-properties-of-the-suppressederror-constructors
info: |
  Properties of the SuppressedError Constructor

  - has a [[Prototype]] internal slot whose value is the intrinsic object %Error%.
features: [explicit-resource-management]
---*/

var proto = Object.getPrototypeOf(SuppressedError);

assert.sameValue(proto, Error);

reportCompare(0, 0);
