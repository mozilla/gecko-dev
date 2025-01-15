// |reftest| shell-option(--enable-math-sumprecise) skip-if(!Math.sumPrecise||!xulRuntime.shell) -- Math.sumPrecise is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-math.sumprecise
description: Math.sumPrecise sums infinities
features: [Math.sumPrecise]
---*/

assert.sameValue(Math.sumPrecise([Infinity]), Infinity);
assert.sameValue(Math.sumPrecise([Infinity, Infinity]), Infinity);
assert.sameValue(Math.sumPrecise([-Infinity]), -Infinity);
assert.sameValue(Math.sumPrecise([-Infinity, -Infinity]), -Infinity);

reportCompare(0, 0);
