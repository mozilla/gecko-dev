// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  DisposableStack return undefned.
features: [explicit-resource-management]
---*/

(function TestDisposableStackDisposeReturnsUndefined() {
    let stack = new DisposableStack();
    assert.sameValue(stack.dispose(), undefined);
})();

reportCompare(0, 0);
