// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: TypeError thrown when options argument is a primitive
features: [BigInt, Symbol, Temporal]
---*/

const badOptions = [
  null,
  true,
  "some string",
  Symbol(),
  1,
  2n,
];

const instance = new Temporal.ZonedDateTime(0n, "UTC");
for (const value of badOptions) {
  assert.throws(TypeError, () => instance.with({ day: 5 }, value),
    `TypeError on wrong options type ${typeof value}`);
  assert.throws(RangeError, () => instance.with({ day: -1 }, value),
    "Partial datetime processed before throwing TypeError");
};

reportCompare(0, 0);
