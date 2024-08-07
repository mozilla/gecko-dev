// |reftest| shell-option(--enable-regexp-escape) skip-if(!RegExp.escape||!xulRuntime.shell) -- RegExp.escape is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-regexp.escape
description: RegExp.escape is a function
info: |
  RegExp.escape is a built-in function of the RegExp object.
features: [RegExp.escape]
---*/

assert.sameValue(typeof RegExp.escape, 'function', 'RegExp.escape is a function');

reportCompare(0, 0);
