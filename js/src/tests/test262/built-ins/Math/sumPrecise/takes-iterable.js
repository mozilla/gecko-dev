// |reftest| shell-option(--enable-math-sumprecise) skip-if(!Math.sumPrecise||!xulRuntime.shell) -- Math.sumPrecise is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-math.sumprecise
description: Math.sumPrecise takes an iterable
features: [generators, Math.sumPrecise]
---*/

assert.sameValue(Math.sumPrecise([1, 2]), 3);

function* gen() {
  yield 1;
  yield 2;
}
assert.sameValue(Math.sumPrecise(gen()), 3);

var overridenArray = [4];
overridenArray[Symbol.iterator] = gen;
assert.sameValue(Math.sumPrecise(overridenArray), 3);

assert.throws(TypeError, function () {
  Math.sumPrecise();
});

assert.throws(TypeError, function () {
  Math.sumPrecise(1, 2);
});

assert.throws(TypeError, function () {
  Math.sumPrecise({});
});

reportCompare(0, 0);
