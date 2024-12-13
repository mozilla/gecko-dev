// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.from
description: Empty object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.assertPlainTime(
  Temporal.PlainTime.from({ hour: 12, minute: 34 }, {}), 12, 34, 0, 0, 0, 0,
  "options may be an empty plain object"
);

TemporalHelpers.assertPlainTime(
  Temporal.PlainTime.from({ hour: 12, minute: 34 }, () => {}), 12, 34, 0, 0, 0, 0,
  "options may be an empty function object"
);

reportCompare(0, 0);
