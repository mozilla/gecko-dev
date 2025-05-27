// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: transferToImmutable throws if receiver is detached ArrayBuffer
esid: sec-arraybuffer.prototype.transfertoimmutable
includes: [detachArrayBuffer.js]
features: [immutable-arraybuffer]
---*/

assert.sameValue(typeof ArrayBuffer.prototype.transferToImmutable, 'function');

var ab = new ArrayBuffer(1);

$DETACHBUFFER(ab);

assert.throws(TypeError, function() {
  ab.transferToImmutable();
});

reportCompare(0, 0);
