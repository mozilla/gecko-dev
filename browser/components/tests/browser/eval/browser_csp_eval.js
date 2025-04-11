/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function eval_allowed_with_special_pref() {
  is(
    window.location.pathname,
    "/content/browser.xhtml",
    "Running in browser.xhtml"
  );

  // eslint-disable-next-line no-eval
  eval("window.test_code_ran = 1;");
  is(window.test_code_ran, 1, "eval() executed successfully");

  delete window.test_code_ran;
});
