// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.constructor
description: Test for Temporal.PlainTime.prototype.constructor.
info: The initial value of Temporal.PlainTime.prototype.constructor is %Temporal.PlainTime%.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.PlainTime.prototype, "constructor", {
  value: Temporal.PlainTime,
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
