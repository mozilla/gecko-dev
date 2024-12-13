// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: >
  BalanceDuration throws a RangeError when the result is too large.
features: [Temporal]
---*/

// Math.trunc(Number.MAX_SAFE_INTEGER / 86400) === 104249991374
var duration = Temporal.Duration.from({days: 104249991374});

assert.throws(RangeError, () => duration.add(duration));

reportCompare(0, 0);
