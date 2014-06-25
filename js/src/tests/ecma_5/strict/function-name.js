/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function fn() {
  return function f(a, b, c) { };
}

assertEq(testLenientAndStrict('var f = fn(); f.name = "g"; f.name',
                              returns("f"), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var f = fn(); delete f.name',
                              returns(false), raisesException(TypeError)),
         true);

reportCompare(true, true);
