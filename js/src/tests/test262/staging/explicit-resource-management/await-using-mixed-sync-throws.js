// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) async -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test exception handling when dispose method throws.
includes: [asyncHelpers.js]
flags: [async]
features: [explicit-resource-management]
---*/

// Dispose method throws -----------------------------
asyncTest(async function() {
  async function TestDisposeMethodThrows() {
    await using x = {
      value: 1,
      [Symbol.asyncDispose]() {
        return 42;
      }
    };

    using y = {
      value: 1,
      [Symbol.dispose]() {
        throw new Test262Error('Symbol.dispose is throwing!');
      }
    };
  };
  await assert.throwsAsync(
      Test262Error, () => TestDisposeMethodThrows(),
      'Symbol.dispose is throwing!');
});
