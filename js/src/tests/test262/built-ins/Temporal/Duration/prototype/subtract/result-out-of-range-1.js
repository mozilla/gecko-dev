// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: >
  BalanceDuration throws a RangeError when the result is too large.
features: [Temporal]
---*/

// Largest temporal unit is "second".
const duration1 = Temporal.Duration.from({seconds: Number.MAX_SAFE_INTEGER});
const duration2 = Temporal.Duration.from({seconds: -Number.MAX_SAFE_INTEGER});

assert.throws(RangeError, () => {
  duration1.subtract(duration2);
});

reportCompare(0, 0);
