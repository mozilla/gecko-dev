// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: verify immutable getter's name property attributes and value
esid: sec-get-arraybuffer.prototype.immutable
includes: [propertyHelper.js]
features: [ArrayBuffer, immutable-arraybuffer]
---*/

var desc = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'immutable');

verifyProperty(desc.get, 'name', {
  value: 'get immutable',
  enumerable: false,
  writable: false,
  configurable: true
});

reportCompare(0, 0);
