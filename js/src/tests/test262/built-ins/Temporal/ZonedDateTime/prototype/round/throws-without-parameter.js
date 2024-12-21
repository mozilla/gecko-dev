// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: Throws without parameter.
features: [Temporal]
---*/

const zdt = new Temporal.ZonedDateTime(217175010123456789n, "+01:00");

assert.throws(TypeError, () => zdt.round());

reportCompare(0, 0);
