// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.RelativeTimeFormat.prototype.resolvedOptions
description: Verifies the branding check for the "resolvedOptions" function of the RelativeTimeFormat prototype object.
info: |
    Intl.RelativeTimeFormat.prototype.resolvedOptions ()

    2. If Type(relativeTimeFormat) is not Object or relativeTimeFormat does not have an [[InitializedRelativeTimeFormat]] internal slot whose value is true, throw a TypeError exception.
features: [Intl.RelativeTimeFormat]
---*/

const fn = Intl.RelativeTimeFormat.prototype.resolvedOptions;

assert.throws(TypeError, () => fn.call(undefined), "undefined");
assert.throws(TypeError, () => fn.call(null), "null");
assert.throws(TypeError, () => fn.call(true), "true");
assert.throws(TypeError, () => fn.call(""), "empty string");
assert.throws(TypeError, () => fn.call(Symbol()), "symbol");
assert.throws(TypeError, () => fn.call(1), "1");
assert.throws(TypeError, () => fn.call({}), "plain object");
assert.throws(TypeError, () => fn.call(Intl.RelativeTimeFormat), "Intl.RelativeTimeFormat");
assert.throws(TypeError, () => fn.call(Intl.RelativeTimeFormat.prototype), "Intl.RelativeTimeFormat.prototype");

reportCompare(0, 0);
