// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: >
  Disregards the calendar if the exact times of the arguments are equal
features: [Temporal]
---*/

const arg1 = new Temporal.ZonedDateTime(1572342398_271_986_102n, "-07:00", "iso8601");
const arg2 = new Temporal.ZonedDateTime(1572342398_271_986_102n, "-07:00", "japanese");
assert.sameValue(Temporal.ZonedDateTime.compare(arg1, arg2), 0);

reportCompare(0, 0);
