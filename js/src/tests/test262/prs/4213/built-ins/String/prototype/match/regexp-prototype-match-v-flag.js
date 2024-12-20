// Copyright (C) 2024 Tan Meng. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-regexp.prototype-@@match
description: RegExp.prototype[@@match] behavior with 'v' flag
features: [Symbol.match, regexp-v-flag]
includes: [compareArray.js]
---*/

const text = '𠮷a𠮷b𠮷';

function doMatch(regex) {
  return RegExp.prototype[Symbol.match].call(regex, text);
}

assert.compareArray(doMatch(/𠮷/g), ["𠮷", "𠮷", "𠮷"], "Basic match with g flag");
assert.compareArray(doMatch(/𠮷/v), ["𠮷"], "Match with v flag");
assert.compareArray(doMatch(/\p{Script=Han}/gv), ["𠮷", "𠮷", "𠮷"], "Unicode property escapes with v flag");
assert.compareArray(doMatch(/./gv), ["𠮷", "a", "𠮷", "b", "𠮷"], "Dot with v flag");
assert.sameValue(doMatch(/x/v), null, "Non-matching regex");

reportCompare(0, 0);
