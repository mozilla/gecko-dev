/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Test case that ensures Array and other list types are not sorted in variables
 * view.
 *
 * The tested types are:
 *  - Array
 *  - Int8Array
 *  - Int16Array
 *  - Int32Array
 *  - Uint8Array
 *  - Uint16Array
 *  - Uint32Array
 *  - Uint8ClampedArray
 *  - Float32Array
 *  - Float64Array
 *  - NodeList
 */

function test() {
  const TEST_URI = "data:text/html;charset=utf-8,   \
    <html>                                          \
      <head>                                        \
        <title>Test document for bug 977500</title> \
      </head>                                       \
      <body>                                        \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      <div></div>                                   \
      </body>                                       \
    </html>";

  let jsterm;

  function* runner() {
    const typedArrayTypes = ["Int8Array", "Int16Array", "Int32Array",
                             "Uint8Array", "Uint16Array", "Uint32Array",
                             "Uint8ClampedArray", "Float32Array",
                             "Float64Array"];

    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);
    jsterm = hud.jsterm;

    // Create an ArrayBuffer of 80 bytes to test TypedArrays. 80 bytes is
    // enough to get 10 items in all different TypedArrays.
    yield jsterm.execute("let buf = new ArrayBuffer(80);");

    // Array
    yield testNotSorted("Array(0,1,2,3,4,5,6,7,8,9,10)");
    // NodeList
    yield testNotSorted("document.querySelectorAll('div')");
    // Object
    yield testSorted("Object({'hello':1,1:5,10:2,4:2,'abc':1})");

    // Typed arrays.
    for (let type of typedArrayTypes) {
      yield testNotSorted("new " + type + "(buf)");
    }
  }

  /**
   * A helper that ensures the properties are not sorted when an object
   * specified by aObject is inspected.
   *
   * @param string aObject
   *        A string that, once executed, creates and returns the object to
   *        inspect.
   */
  function* testNotSorted(aObject) {
    info("Testing " + aObject);
    let deferred = promise.defer();
    jsterm.once("variablesview-fetched", (_, aVar) => deferred.resolve(aVar));
    jsterm.execute("inspect(" + aObject + ")");

    let variableScope = yield deferred.promise;
    ok(variableScope, "Variables view opened");

    // If the properties are sorted: keys = ["0", "1", "10",...] <- incorrect
    // If the properties are not sorted: keys = ["0", "1", "2",...] <- correct
    let keyIterator = variableScope._store.keys();
    is(keyIterator.next().value, "0", "First key is 0");
    is(keyIterator.next().value, "1", "Second key is 1");

    // If the properties are sorted, the next one will be 10.
    is(keyIterator.next().value, "2", "Third key is 2, not 10");
  }
  /**
   * A helper that ensures the properties are sorted when an object
   * specified by aObject is inspected.
   *
   * @param string aObject
   *        A string that, once executed, creates and returns the object to
   *        inspect.
   */
  function* testSorted(aObject) {
    info("Testing " + aObject);
    let deferred = promise.defer();
    jsterm.once("variablesview-fetched", (_, aVar) => deferred.resolve(aVar));
    jsterm.execute("inspect(" + aObject + ")");

    let variableScope = yield deferred.promise;
    ok(variableScope, "Variables view opened");

    // If the properties are sorted:
    // keys = ["1", "4", "10",..., "abc", "hello"] <- correct
    // If the properties are not sorted:
    // keys = ["1", "10", "4",...] <- incorrect
    let keyIterator = variableScope._store.keys();
    is(keyIterator.next().value, "1", "First key should be 1");
    is(keyIterator.next().value, "4", "Second key should be 4");

    // If the properties are sorted, the next one will be 10.
    is(keyIterator.next().value, "10", "Third key is 10");
    // If sorted next properties should be "abc" then "hello"
    is(keyIterator.next().value, "abc", "Fourth key is abc");
    is(keyIterator.next().value, "hello", "Fifth key is hello");
  }

  Task.spawn(runner).then(finishTest);
}
