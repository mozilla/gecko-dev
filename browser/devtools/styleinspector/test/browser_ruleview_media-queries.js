/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we correctly display appropriate media query titles in the
// rule view.

const TEST_URI = TEST_URL_ROOT + "doc_media_queries.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();

  yield selectNode("div", inspector);

  let elementStyle = view._elementStyle;

  let _strings = Services.strings
    .createBundle("chrome://global/locale/devtools/styleinspector.properties");

  let inline = _strings.GetStringFromName("rule.sourceInline");

  is(elementStyle.rules.length, 3, "Should have 3 rules.");
  is(elementStyle.rules[0].title, inline, "check rule 0 title");
  is(elementStyle.rules[1].title, inline +
    ":15 @media screen and (min-width: 1px)", "check rule 1 title");
  is(elementStyle.rules[2].title, inline + ":8", "check rule 2 title");
});

