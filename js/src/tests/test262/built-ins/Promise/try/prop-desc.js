// |reftest| shell-option(--enable-promise-try) skip-if(!Promise.try||!xulRuntime.shell) -- promise-try is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband. All rights reserved.
// See LICENSE for details.

/*---
author: Jordan Harband
description: Promise.try property descriptor
features: [promise-try]
includes: [propertyHelper.js]
---*/

verifyProperty(Promise, 'try', {
  value: Promise.try,
  writable: true,
  enumerable: false,
  configurable: true
})

reportCompare(0, 0);
