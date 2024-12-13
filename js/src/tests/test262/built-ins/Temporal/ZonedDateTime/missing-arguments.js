// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: TypeError thrown when constructor invoked with no argument
features: [Temporal]
---*/

assert.throws(TypeError, () => new Temporal.ZonedDateTime());

reportCompare(0, 0);
