// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant
description: >
  Throws a RangeError if the input is far away from the epoch nanoseconds limits.
features: [Temporal]
---*/

assert.throws(
  RangeError,
  () => new Temporal.Instant(2n ** 128n),
  "2n ** 128n"
);

assert.throws(
  RangeError,
  () => new Temporal.Instant(-(2n ** 128n)),
  "-(2n ** 128n)"
);

reportCompare(0, 0);
