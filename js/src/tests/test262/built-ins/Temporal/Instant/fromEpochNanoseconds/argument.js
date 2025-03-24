// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.fromepochnanoseconds
description: >
  TypeError thrown if input is of wrong primitive type.
info: |
  Temporal.Instant.fromEpochNanoseconds ( epochNanoseconds )

  1. Set epochNanoseconds to ? ToBigInt(epochNanoseconds).
  ...
features: [Temporal]
---*/

assert.throws(TypeError, () => Temporal.Instant.fromEpochNanoseconds(), "undefined");
assert.throws(TypeError, () => Temporal.Instant.fromEpochNanoseconds(undefined), "undefined");
assert.throws(TypeError, () => Temporal.Instant.fromEpochNanoseconds(null), "null");
assert.throws(TypeError, () => Temporal.Instant.fromEpochNanoseconds(42), "number");
assert.throws(TypeError, () => Temporal.Instant.fromEpochNanoseconds(Symbol()), "symbol");

reportCompare(0, 0);
