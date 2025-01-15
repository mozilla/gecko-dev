// |reftest| shell-option(--enable-math-sumprecise) skip-if(!Math.sumPrecise||!xulRuntime.shell) -- Math.sumPrecise is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-math.sumprecise
description: Math.sumPrecise returns -0 on an empty list or list of all -0
features: [Math.sumPrecise]
---*/

assert.sameValue(Math.sumPrecise([]), -0);
assert.sameValue(Math.sumPrecise([-0]), -0);
assert.sameValue(Math.sumPrecise([-0, -0]), -0);
assert.sameValue(Math.sumPrecise([-0, 0]), 0);

reportCompare(0, 0);
