// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.constructor
description: Test for Temporal.PlainMonthDay.prototype.constructor.
info: The initial value of Temporal.PlainMonthDay.prototype.constructor is %Temporal.PlainMonthDay%.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.PlainMonthDay.prototype, "constructor", {
  value: Temporal.PlainMonthDay,
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
