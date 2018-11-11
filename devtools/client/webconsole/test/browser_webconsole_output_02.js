/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the webconsole output for various types of objects.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-output-02.html";

var inputTests = [
  // 0 - native named function
  {
    input: "document.getElementById",
    output: "function getElementById()",
    printOutput: "function getElementById() {\n    [native code]\n}",
    inspectable: true,
    variablesViewLabel: "getElementById()",
  },

  // 1 - anonymous function
  {
    input: "(function() { return 42; })",
    output: "function ()",
    printOutput: "function () { return 42; }",
    suppressClick: true
  },

  // 2 - named function
  {
    input: "window.testfn1",
    output: "function testfn1()",
    printOutput: "function testfn1() { return 42; }",
    suppressClick: true
  },

  // 3 - anonymous function, but spidermonkey gives us an inferred name.
  {
    input: "testobj1.testfn2",
    output: "function testobj1.testfn2()",
    printOutput: "function () { return 42; }",
    suppressClick: true
  },

  // 4 - named function with custom display name
  {
    input: "window.testfn3",
    output: "function testfn3DisplayName()",
    printOutput: "function testfn3() { return 42; }",
    suppressClick: true
  },

  // 5 - basic array
  {
    input: "window.array1",
    output: 'Array [ 1, 2, 3, "a", "b", "c", "4", "5" ]',
    printOutput: "1,2,3,a,b,c,4,5",
    inspectable: true,
    variablesViewLabel: "Array[8]",
  },

  // 6 - array with objects
  {
    input: "window.array2",
    output: 'Array [ "a", HTMLDocument \u2192 test-console-output-02.html, ' +
            "<body>, DOMStringMap[0], DOMTokenList[0] ]",
    printOutput: '"a,[object HTMLDocument],[object HTMLBodyElement],' +
                 '[object DOMStringMap],"',
    inspectable: true,
    variablesViewLabel: "Array[5]",
  },

  // 7 - array with more than 10 elements
  {
    input: "window.array3",
    output: "Array [ 1, Window \u2192 test-console-output-02.html, null, " +
            '"a", "b", undefined, false, "", -Infinity, ' +
            "testfn3DisplayName(), 3 more\u2026 ]",
    printOutput: '"1,[object Window],,a,b,,false,,-Infinity,' +
                 'function testfn3() { return 42; },[object Object],foo,bar"',
    inspectable: true,
    variablesViewLabel: "Array[13]",
  },

  // 8 - array with holes and a cyclic reference
  {
    input: "window.array4",
    output: 'Array [ <5 empty slots>, "test", Array[7] ]',
    printOutput: '",,,,,test,"',
    inspectable: true,
    variablesViewLabel: "Array[7]",
  },

  // 9
  {
    input: "window.typedarray1",
    output: "Int32Array [ 1, 287, 8651, 40983, 8754 ]",
    printOutput: "1,287,8651,40983,8754",
    inspectable: true,
    variablesViewLabel: "Int32Array[5]",
  },

  // 10 - Set with cyclic reference
  {
    input: "window.set1",
    output: 'Set [ 1, 2, null, Array[13], "a", "b", undefined, <head>, ' +
            "Set[9] ]",
    printOutput: "[object Set]",
    inspectable: true,
    variablesViewLabel: "Set[9]",
  },

  // 11 - Object with cyclic reference and a getter
  {
    input: "window.testobj2",
    output: 'Object { a: "b", c: "d", e: 1, f: "2", foo: Object, ' +
            "bar: Object, getterTest: Getter }",
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Object",
  },

  // 12 - Object with more than 10 properties
  {
    input: "window.testobj3",
    output: 'Object { a: "b", c: "d", e: 1, f: "2", g: true, h: null, ' +
            'i: undefined, j: "", k: StyleSheetList[0], l: NodeList[5], ' +
            "2 more\u2026 }",
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Object",
  },

  // 13 - Object with a non-enumerable property that we do not show
  {
    input: "window.testobj4",
    output: 'Object { a: "b", c: "d", 1 more\u2026 }',
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Object",
  },

  // 14 - Map with cyclic references
  {
    input: "window.map1",
    output: 'Map { a: "b", HTMLCollection[2]: Object, Map[3]: Set[9] }',
    printOutput: "[object Map]",
    inspectable: true,
    variablesViewLabel: "Map[3]",
  },

  // 15 - WeakSet
  {
    input: "window.weakset",
    // Need a regexp because the order may vary.
    output: new RegExp("WeakSet \\[ (String, <head>|<head>, String) \\]"),
    printOutput: "[object WeakSet]",
    inspectable: true,
    variablesViewLabel: "WeakSet[2]",
  },

  // 16 - WeakMap
  {
    input: "window.weakmap",
    // Need a regexp because the order may vary.
    output: new RegExp("WeakMap { (String: 23, HTMLCollection\\[2\\]: Object|HTMLCollection\\[2\\]: Object, String: 23) }"),
    printOutput: "[object WeakMap]",
    inspectable: true,
    variablesViewLabel: "WeakMap[2]",
  },
];

function test() {
  requestLongerTimeout(2);
  Task.spawn(function* () {
    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);
    yield checkOutputForInputs(hud, inputTests);
    inputTests = null;
  }).then(finishTest);
}
