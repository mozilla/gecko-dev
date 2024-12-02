/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Copyright (C) 2024 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

async function f() {
  let
  await 0;
}

reportCompare(true, f instanceof Function);
