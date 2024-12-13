// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.constructor
description: An ISO string is not valid input for a constructor's calendar param
features: [Temporal]
---*/

assert.throws(
  RangeError,
  () => new Temporal.ZonedDateTime(0n, "UTC", "1997-12-04[u-ca=iso8601]"),
  "An ISO string is not a valid calendar ID for constructor parameter"
);

reportCompare(0, 0);
