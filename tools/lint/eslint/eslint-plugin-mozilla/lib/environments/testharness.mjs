/**
 * @file Defines the environment for testharness.js files. This
 * is automatically included in (x)html files including
 * /resources/testharness.js.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// These globals are taken from dom/imptests/testharness.js, via the expose
// function.

export default {
  globals: {
    EventWatcher: "readonly",
    test: "readonly",
    async_test: "readonly",
    promise_test: "readonly",
    promise_rejects: "readonly",
    generate_tests: "readonly",
    setup: "readonly",
    done: "readonly",
    on_event: "readonly",
    step_timeout: "readonly",
    format_value: "readonly",
    assert_true: "readonly",
    assert_false: "readonly",
    assert_equals: "readonly",
    assert_not_equals: "readonly",
    assert_in_array: "readonly",
    assert_object_equals: "readonly",
    assert_array_equals: "readonly",
    assert_approx_equals: "readonly",
    assert_less_than: "readonly",
    assert_greater_than: "readonly",
    assert_between_exclusive: "readonly",
    assert_less_than_equal: "readonly",
    assert_greater_than_equal: "readonly",
    assert_between_inclusive: "readonly",
    assert_regexp_match: "readonly",
    assert_class_string: "readonly",
    assert_exists: "readonly",
    assert_own_property: "readonly",
    assert_not_exists: "readonly",
    assert_inherits: "readonly",
    assert_idl_attribute: "readonly",
    assert_readonly: "readonly",
    assert_throws: "readonly",
    assert_unreaded: "readonly",
    assert_any: "readonly",
    fetch_tests_from_worker: "readonly",
    timeout: "readonly",
    add_start_callback: "readonly",
    add_test_state_callback: "readonly",
    add_result_callback: "readonly",
    add_completion_callback: "readonly",
  },
};
