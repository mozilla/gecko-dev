// Copyright 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-pluralruleselect
description: Checks that the notation option is used appropriately in the select method.
info: |
  PluralRuleSelect (
    _locale_: a language tag,
    _type_: *"cardinal"* or *"ordinal"*,
    _notation_: a String,
    _s_: a decimal String,
  ): *"zero"*, *"one"*, *"two"*, *"few"*, *"many"*, or *"other"*
    ...
    The returned String characterizes the plural category of _s_ according to _locale_, _type_ and _notation_.
    ...
---*/

assert.sameValue(new Intl.PluralRules('sl', { notation: 'compact' }).select(1.00000020e6), 'one', 'compact notation');
assert.sameValue(new Intl.PluralRules('sl', { notation: 'standard' }).select(1.00000020e6), 'other', 'standard notation');

reportCompare(0, 0);
