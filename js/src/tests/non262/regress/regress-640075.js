/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

"use strict";
assertThrowsInstanceOf(
    () => eval("(function() { eval(); function eval() {} })"),
    SyntaxError
)

reportCompare(0, 0, "ok");
