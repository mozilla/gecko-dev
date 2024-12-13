// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: Calendar ID is canonicalized
features: [Temporal]
---*/

[
  "2024-07-02[u-ca=islamicc]",
  { year: 1445, month: 12, day: 25, calendar: "islamicc" },
].forEach((arg) => {
  const result = Temporal.PlainDate.from(arg);
  assert.sameValue(result.calendarId, "islamic-civil", "calendar ID is canonicalized");
});

reportCompare(0, 0);
