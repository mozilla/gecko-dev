// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: Calendar ID is canonicalized
features: [Temporal]
---*/

const instance = new Temporal.PlainMonthDay(2, 11, "islamic-civil", 1972);

[
  "1972-02-11[u-ca=islamicc]",
  { monthCode: "M12", day: 25, calendar: "islamicc" },
].forEach((arg) => assert(instance.equals(arg), "calendar ID is canonicalized"));

reportCompare(0, 0);
