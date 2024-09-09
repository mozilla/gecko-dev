// |reftest| shell-option(--enable-promise-try) skip-if(!Promise.try||!xulRuntime.shell) -- promise-try is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Promise.try errors when the receiver is not a constructor
esid: sec-promise.try
features: [promise-try]
---*/

assert.throws(TypeError, function () {
  Promise.try.call(eval);
});

reportCompare(0, 0);
