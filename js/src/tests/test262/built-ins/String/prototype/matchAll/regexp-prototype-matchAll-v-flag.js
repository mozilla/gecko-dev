// Copyright (C) 2024 Tan Meng. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-regexp.prototype-@@matchall
description: RegExp.prototype[@@matchAll] behavior with 'v' flag
features: [Symbol.matchAll, regexp-v-flag]
---*/

const text = '𠮷a𠮷b𠮷';

function doMatchAll(regex) {
  return Array.from(RegExp.prototype[Symbol.matchAll].call(regex, text), m => [m[0], m.index]);
}

assert.sameValue(
  doMatchAll(/𠮷/g).toString(),
  "𠮷,0,𠮷,3,𠮷,6",
  "Basic matchAll with g flag"
);

assert.sameValue(
  doMatchAll(/𠮷/gv).toString(),
  "𠮷,0,𠮷,3,𠮷,6",
  "matchAll with v flag"
);

assert.sameValue(
  doMatchAll(/\p{Script=Han}/gv).toString(),
  "𠮷,0,𠮷,3,𠮷,6",
  "Unicode property escapes with v flag"
);

assert.sameValue(
  doMatchAll(/./gv).toString(),
  "𠮷,0,a,2,𠮷,3,b,5,𠮷,6",
  "Dot with v flag"
);

assert.sameValue(
  doMatchAll(/(?:)/gv).length,
  6,
  "Empty matches with v flag"
);

reportCompare(0, 0);
