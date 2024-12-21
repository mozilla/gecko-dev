// |reftest| shell-option(--enable-error-iserror) skip-if(!Error.isError||!xulRuntime.shell) -- Error.isError is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-error.iserror
description: >
  The initial value of Error.isError.name is "isError".
features: [Error.isError]
---*/

assert.sameValue(Error.isError.name, 'isError');


reportCompare(0, 0);
