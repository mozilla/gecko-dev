// Copyright 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializepluralrules
description: Checks that the notation option is picked up correctly.
info: |
  Intl.PluralRules ( [ _locales_ [ , _options_ ] ] )
    ...
    1. Let notation be ? GetOption(options, "notation", "string", « "standard", "compact", "scientific", "engineering" », "standard").
    ...
---*/

const validValues = ["standard", "compact", "scientific", "engineering", new String("standard"), new String("compact"), new String("scientific"), new String("engineering")];
const invalidValues = ["COMPACT", "ståndard", 123, false, Symbol("foo"), null, {}, [], ""];

for (const value of validValues) {
  const pr = new Intl.PluralRules("en", { notation: value });
  assert(pr.resolvedOptions().notation === value, `Resolved options should have notation ${value}`);
}
for (const value of invalidValues) {
  assert.throws(RangeError, () => {
    new Intl.PluralRules("en", { notation: value });
  }, `Exception should be thrown for ${value}`);
}

reportCompare(0, 0);
