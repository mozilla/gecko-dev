// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.compare
description: Empty object may be used as options
features: [Temporal]
---*/

assert.sameValue(
  Temporal.Duration.compare({ hours: 1 }, { minutes: 60 }, {}), 0,
  "options may be an empty plain object"
);

assert.sameValue(
  Temporal.Duration.compare({ hours: 1 }, { minutes:60 }, () => {}), 0,
  "options may be an empty function object"
);

reportCompare(0, 0);
