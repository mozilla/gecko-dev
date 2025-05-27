// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: verify immutable property requires ArrayBuffer receiver
esid: sec-get-arraybuffer.prototype.immutable
features: [ArrayBuffer, immutable-arraybuffer]
---*/

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.immutable;
});

reportCompare(0, 0);
