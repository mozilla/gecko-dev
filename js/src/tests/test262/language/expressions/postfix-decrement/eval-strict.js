// |reftest| error:SyntaxError
'use strict';
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.3.2-2-2-s
esid: sec-postfix-decrement-operator
description: >
    Strict Mode - SyntaxError is thrown if the identifier 'eval'
    appear as a PostfixExpression(eval--)
flags: [onlyStrict]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

eval--;
