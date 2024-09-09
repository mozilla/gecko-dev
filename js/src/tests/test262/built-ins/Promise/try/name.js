// |reftest| shell-option(--enable-promise-try) skip-if(!Promise.try||!xulRuntime.shell) -- promise-try is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Promise.try `name` property
includes: [propertyHelper.js]
features: [promise-try]
---*/

verifyProperty(Promise.try, "name", {
  value: "try",
  writable: false,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
