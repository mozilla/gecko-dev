// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.subtract
description: Empty or a function object may be used as options
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(0n, "UTC");

const result1 = instance.subtract({ years: 1 }, {});
assert.sameValue(
  result1.epochNanoseconds, -31536000000000000n, "UTC",
  "options may be an empty plain object"
);

const result2 = instance.subtract({ years: 1 }, () => {});
assert.sameValue(
  result2.epochNanoseconds, -31536000000000000n, "UTC",
  "options may be a function object"
);

reportCompare(0, 0);
