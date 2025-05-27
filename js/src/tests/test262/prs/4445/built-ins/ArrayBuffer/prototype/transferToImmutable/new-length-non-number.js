// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: transferToImmutable throws if new length is not a number
esid: sec-arraybuffer.prototype.transfertoimmutable
features: [immutable-arraybuffer]
---*/

var log = [];
var newLength = {
  toString: function() {
    log.push('toString');
    return {};
  },
  valueOf: function() {
    log.push('valueOf');
    return {};
  }
};
var ab = new ArrayBuffer(0);

assert.throws(TypeError, function() {
  ab.transferToImmutable(newLength);
});

assert.sameValue(log.length, 2);
assert.sameValue(log[0], 'valueOf');
assert.sameValue(log[1], 'toString');

reportCompare(0, 0);
