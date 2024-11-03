// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) async -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test if disposed methods are called correctly in async function.
includes: [asyncHelpers.js, compareArray.js]
flags: [async]
features: [explicit-resource-management]
---*/

// FunctionBody --------------
asyncTest(async function() {
  let functionBodyValues = [];

  async function TestUsingInFunctionBody() {
    await using x = {
      value: 1,
      [Symbol.asyncDispose]() {
        functionBodyValues.push(42);
      }
    };
    await using y = {
      value: 2,
      [Symbol.asyncDispose]() {
        functionBodyValues.push(43);
      }
    };
  }

  functionBodyValues = [];
  await TestUsingInFunctionBody();
  assert.compareArray(functionBodyValues, [43, 42]);
});
