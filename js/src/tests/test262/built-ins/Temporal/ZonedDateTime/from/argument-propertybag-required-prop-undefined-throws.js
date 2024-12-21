// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: from() throws if a required property is undefined.
features: [Temporal]
---*/

assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  year: 1976,
  month: undefined,
  monthCode: undefined,
  day: 18,
  timeZone: "+01:00"
}));


reportCompare(0, 0);
