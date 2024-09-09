// |reftest| shell-option(--enable-promise-try) skip-if(!Promise.try||!xulRuntime.shell) -- promise-try is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    `Promise.try` invoked on a constructor value that throws an error
features: [promise-try]
---*/

var CustomPromise = function () {
  throw new Test262Error();
};

assert.throws(Test262Error, function () {
  Promise.try.call(CustomPromise, function () {});
});

reportCompare(0, 0);
