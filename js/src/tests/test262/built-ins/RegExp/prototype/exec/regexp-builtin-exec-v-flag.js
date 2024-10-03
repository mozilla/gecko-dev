// Copyright (C) 2024 Tan Meng. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-regexpbuiltinexec
description: RegExpBuiltinExec behavior with 'v' flag
features: [regexp-v-flag]
includes: [compareArray.js]
---*/

const text = '𠮷a𠮷b𠮷';

function doExec(regex) {
  const result = regex.exec(text);
  return result ? [result[0], result.index] : null;
}

assert.compareArray(doExec(/𠮷/), ["𠮷", 0], "Basic exec without v flag");
assert.compareArray(doExec(/𠮷/v), ["𠮷", 0], "Exec with v flag");
assert.compareArray(doExec(/\p{Script=Han}/v), ["𠮷", 0], "Unicode property escapes with v flag");
assert.compareArray(doExec(/./v), ["𠮷", 0], "Dot with v flag");
assert.sameValue(doExec(/x/v), null, "Non-matching regex");

const regexWithGroups = /(\p{Script=Han})(.)/v;
const resultWithGroups = regexWithGroups.exec(text);
assert.sameValue(resultWithGroups[1], "𠮷", "Capture group 1");
assert.sameValue(resultWithGroups[2], "a", "Capture group 2");
assert.sameValue(resultWithGroups.index, 0, "Match index for groups");

reportCompare(0, 0);
