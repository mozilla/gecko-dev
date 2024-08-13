// |reftest| shell-option(--enable-regexp-modifiers) skip-if(release_or_beta||!xulRuntime.shell) error:SyntaxError -- regexp-modifiers is not released yet, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: It is a Syntax Error if the any code point in the source text matched by the first RegularExpressionFlags is also contained in the source text matched by the second RegularExpressionFlags. (arithmetic regular expression flags)
esid: sec-patterns-static-semantics-early-errors
features: [regexp-modifiers]
negative:
  phase: parse
  type: SyntaxError
info: |
    Atom :: ( ? RegularExpressionFlags - RegularExpressionFlags : Disjunction )
    ...

---*/

$DONOTEVALUATE();

/(?s-s:a)//*{ global-modifiers }*/;
