// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.tostring
description: Serializing balance doesn't lose precision when values are precise.
features: [Temporal]
---*/

const d = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0,
                                Number.MAX_SAFE_INTEGER,
                                Number.MAX_SAFE_INTEGER, 0);

assert.sameValue(d.toString(), "PT9016206453995.731991S");

reportCompare(0, 0);
