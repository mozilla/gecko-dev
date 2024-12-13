// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.tostring
description: If calendarName is "always", the calendar ID should be included.
features: [Temporal]
---*/

const monthday = new Temporal.PlainMonthDay(5, 2);
const result = monthday.toString({ calendarName: "always" });
assert.sameValue(result, "1972-05-02[u-ca=iso8601]", `built-in ISO calendar for calendarName = always`);

reportCompare(0, 0);
