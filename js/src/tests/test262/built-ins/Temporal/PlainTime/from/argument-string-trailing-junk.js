// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.from
description: RangeError thrown if a string argument has trailing junk
features: [Temporal, arrow-function]
---*/

const arg = "15:23:30.100junk";
assert.throws(RangeError, () => Temporal.PlainTime.from(arg));

reportCompare(0, 0);
