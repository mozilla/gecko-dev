// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Kate Miháliková. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tolocalestring
description: >
    toLocaleString return a string.
features: [Temporal]
---*/

const date = new Temporal.PlainDate(2000, 5, 2);

assert.sameValue(typeof date.toLocaleString("en", { dateStyle: "short" }), "string");

reportCompare(0, 0);
