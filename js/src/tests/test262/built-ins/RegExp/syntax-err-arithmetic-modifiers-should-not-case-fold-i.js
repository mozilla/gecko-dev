// |reftest| shell-option(--enable-regexp-modifiers) skip-if(release_or_beta||!xulRuntime.shell) -- regexp-modifiers is not released yet, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Code points other than "i", "m", "s" should not be case folded to "i", "m", or "s" (arithmetic regular expression flags)
esid: sec-patterns-static-semantics-early-errors
features: [regexp-modifiers]
info: |
    Atom :: ( ? RegularExpressionFlags - RegularExpressionFlags : Disjunction )
    ...

---*/

assert.throws(SyntaxError, function () {
  RegExp("(?-I:a)", "i");
}, 'RegExp("(?-I:a)", "i"): ');

reportCompare(0, 0);
