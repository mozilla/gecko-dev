/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {parseSingleValue} = require("devtools/shared/css/parsing-utils");
const {isCssPropertyKnown} = require("devtools/server/actors/css-properties");

const TEST_DATA = [
  {input: null, throws: true},
  {input: undefined, throws: true},
  {input: "", expected: {value: "", priority: ""}},
  {input: "  \t \t \n\n  ", expected: {value: "", priority: ""}},
  {input: "blue", expected: {value: "blue", priority: ""}},
  {input: "blue !important", expected: {value: "blue", priority: "important"}},
  {input: "blue!important", expected: {value: "blue", priority: "important"}},
  {input: "blue ! important", expected: {value: "blue", priority: "important"}},
  {
    input: "blue !  important",
    expected: {value: "blue", priority: "important"}
  },
  {input: "blue !", expected: {value: "blue", priority: ""}},
  {input: "blue !mportant", expected: {value: "blue !mportant", priority: ""}},
  {
    input: "  blue   !important ",
    expected: {value: "blue", priority: "important"}
  },
  {
    input: "url(\"http://url.com/whyWouldYouDoThat!important.png\") !important",
    expected: {
      value: "url(\"http://url.com/whyWouldYouDoThat!important.png\")",
      priority: "important"
    }
  },
  {
    input: "url(\"http://url.com/whyWouldYouDoThat!important.png\")",
    expected: {
      value: "url(\"http://url.com/whyWouldYouDoThat!important.png\")",
      priority: ""
    }
  },
  {
    input: "\"content!important\" !important",
    expected: {
      value: "\"content!important\"",
      priority: "important"
    }
  },
  {
    input: "\"content!important\"",
    expected: {
      value: "\"content!important\"",
      priority: ""
    }
  },
  {
    input: "\"all the \\\"'\\\\ special characters\"",
    expected: {
      value: "\"all the \\\"'\\\\ special characters\"",
      priority: ""
    }
  }
];

function run_test() {
  for (let test of TEST_DATA) {
    do_print("Test input value " + test.input);
    try {
      let output = parseSingleValue(isCssPropertyKnown, test.input);
      assertOutput(output, test.expected);
    } catch (e) {
      do_print("parseSingleValue threw an exception with the given input " +
        "value");
      if (test.throws) {
        do_print("Exception expected");
        do_check_true(true);
      } else {
        do_print("Exception unexpected\n" + e);
        do_check_true(false);
      }
    }
  }
}

function assertOutput(actual, expected) {
  do_print("Check that the output has the expected value and priority");
  do_check_eq(expected.value, actual.value);
  do_check_eq(expected.priority, actual.priority);
}
