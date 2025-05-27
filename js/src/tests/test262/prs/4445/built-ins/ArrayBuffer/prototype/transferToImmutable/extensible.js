// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: transferToImmutable is extensible
esid: sec-arraybuffer.prototype.transfertoimmutable
features: [immutable-arraybuffer]
---*/

assert(Object.isExtensible(ArrayBuffer.prototype.transferToImmutable));

reportCompare(0, 0);
