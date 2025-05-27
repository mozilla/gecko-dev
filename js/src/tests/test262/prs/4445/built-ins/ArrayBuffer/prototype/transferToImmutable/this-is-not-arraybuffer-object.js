// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: transferToImmutable throws if receiver is not ArrayBuffer
esid: sec-arraybuffer.prototype.transfertoimmutable
features: [immutable-arraybuffer]
---*/

assert.sameValue(typeof ArrayBuffer.prototype.transferToImmutable, 'function');

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.transferToImmutable();
}, '`this` value is the ArrayBuffer prototype');

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.transferToImmutable.call({});
}, '`this` value is an object');

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.transferToImmutable.call([]);
}, '`this` value is an array');

reportCompare(0, 0);
