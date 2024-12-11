// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth
description: Calendar argument defaults to the built-in ISO 8601 calendar
features: [Temporal]
---*/

const args = [2000, 5];

const dateExplicit = new Temporal.PlainYearMonth(...args, undefined);
assert.sameValue(dateExplicit.calendarId, "iso8601");

const dateImplicit = new Temporal.PlainYearMonth(...args);
assert.sameValue(dateImplicit.calendarId, "iso8601");

reportCompare(0, 0);
