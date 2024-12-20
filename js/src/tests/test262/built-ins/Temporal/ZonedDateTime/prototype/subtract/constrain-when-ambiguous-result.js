// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.subtract
description: Constrains when ambiguous result.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const mar31 = Temporal.ZonedDateTime.from("2020-03-31T15:00+00:00[UTC]");
const expected = Temporal.ZonedDateTime.from("2020-02-29T15:00:00+00:00[UTC]");

TemporalHelpers.assertZonedDateTimesEqual(
    mar31.subtract({ months: 1 }),
    expected);

TemporalHelpers.assertZonedDateTimesEqual(
    mar31.subtract({ months: 1 }, { overflow: "constrain" }),
    expected);


reportCompare(0, 0);
