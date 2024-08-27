/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for variables in rule view.

const TEST_URI = URL_ROOT + "doc_variables_1.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#target", inspector);

  info(
    "Tests basic support for CSS Variables for both single variable " +
      "and double variable. Formats tested: var(x, constant), var(x, var(y))"
  );

  const unsetColor = getRuleViewProperty(
    view,
    "div",
    "color"
  ).valueSpan.querySelector(".ruleview-unmatched");
  is(unsetColor.textContent, " red", "red is unmatched in color");

  await assertVariableTooltipForProperty(view, "div", "color", {
    header: "chartreuse",
    // Computed value isn't displayed when it's the same as we put in the header
    computed: null,
  });

  await assertVariableTooltipForProperty(view, "div", "background-color", {
    index: 0,
    header: "--not-set is not set",
    isMatched: false,
  });

  await assertVariableTooltipForProperty(view, "div", "background-color", {
    index: 1,
    header: "seagreen",
    // Computed value isn't displayed when it's the same as we put in the header
    computed: null,
  });

  await assertVariableTooltipForProperty(view, "div", "outline-color", {
    header: "var(--color)",
    computed: "chartreuse",
  });

  await assertVariableTooltipForProperty(view, "div", "border-color", {
    header: "var(--theme-color)",
    computed: "light-dark(chartreuse, seagreen)",
  });

  await assertVariableTooltipForProperty(view, "div", "background", {
    header: "var(--empty)",
    computed: "<empty>",
  });

  await assertVariableTooltipForProperty(view, "*", "--nested-with-empty", {
    header: "<empty>",
  });
});
