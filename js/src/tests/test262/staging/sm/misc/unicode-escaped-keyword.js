/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/
function throws(code) {
    var type;
    try {
        eval(code);
    } catch (ex) {
        type = ex.name;
    }
    assert.sameValue(type, 'SyntaxError');
}

var s = '\\u0073';
throws('var thi' + s);
throws('switch (' + s + 'witch) {}')
throws('var ' + s + 'witch');


reportCompare(0, 0);
