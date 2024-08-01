// GENERATED, DO NOT EDIT
// file: hidden-constructors.js
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  Provides uniform access to built-in constructors that are not exposed to the global object.
defines:
  - AsyncFunction
  - AsyncGeneratorFunction
  - GeneratorFunction
---*/

var AsyncFunction;
var AsyncGeneratorFunction;
var GeneratorFunction;

try {
  AsyncFunction = Object.getPrototypeOf(new Function('return async function dummy() {}')()).constructor;
} catch(e) {}

try {
  AsyncGeneratorFunction = Object.getPrototypeOf(new Function('return async function* dummy() {}')()).constructor;
} catch(e) {}

try {
  GeneratorFunction = Object.getPrototypeOf(new Function('return function* dummy() {}')()).constructor;
} catch(e) {}

// file: isConstructor.js
// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
    Test if a given function is a constructor function.
defines: [isConstructor]
features: [Reflect.construct]
---*/

function isConstructor(f) {
    if (typeof f !== "function") {
      throw new Test262Error("isConstructor invoked with a non-function value");
    }

    try {
        Reflect.construct(function(){}, [], f);
    } catch (e) {
        return false;
    }
    return true;
}
