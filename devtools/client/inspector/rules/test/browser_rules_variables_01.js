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
    header:
      // prettier-ignore
      '<span xmlns="http://www.w3.org/1999/xhtml" data-color="chartreuse">' +
        '<span class="ruleview-swatch ruleview-colorswatch" style="background-color:chartreuse" tabindex="0" role="button">' +
        '</span>' +
        '<span class="ruleview-color">chartreuse</span>' +
      '</span>',
    // Computed value isn't displayed when it's the same as we put in the header
    computed: null,
  });

  await assertVariableTooltipForProperty(view, "div", "background-color", {
    index: 0,
    header: "--not-set is not set",
    headerClasses: [],
    isMatched: false,
  });

  await assertVariableTooltipForProperty(view, "div", "background-color", {
    index: 1,
    header:
      // prettier-ignore
      '<span xmlns="http://www.w3.org/1999/xhtml" data-color="seagreen">' +
        '<span class="ruleview-swatch ruleview-colorswatch" style="background-color:seagreen" tabindex="0" role="button">' +
        '</span>' +
        '<span class="ruleview-color">seagreen</span>' +
      '</span>',
    // Computed value isn't displayed when it's the same as we put in the header
    computed: null,
  });

  await assertVariableTooltipForProperty(view, "div", "outline-color", {
    header:
      // prettier-ignore
      '<span xmlns="http://www.w3.org/1999/xhtml" data-color="chartreuse">' +
        '<span class="ruleview-swatch ruleview-colorswatch" style="background-color:chartreuse" tabindex="0" role="button">' +
        '</span>' +
        '<span class="ruleview-color">' +
          'var(' +
            '<span data-variable="chartreuse" class="ruleview-variable">--color</span>' +
          ')' +
        '</span>' +
      '</span>',
    computed:
      // prettier-ignore
      `<span xmlns="http://www.w3.org/1999/xhtml" data-color="chartreuse">` +
        `<span ` +
          `class="ruleview-swatch ruleview-colorswatch" ` +
          `style="background-color:chartreuse" ` +
          `tabindex="0" ` +
          `role="button">` +
        `</span>` +
        `<span class="ruleview-color">chartreuse</span>` +
      `</span>`,
  });

  await assertVariableTooltipForProperty(view, "div", "border-color", {
    header:
      // prettier-ignore
      '<span xmlns="http://www.w3.org/1999/xhtml">' +
        'var(' +
          '<span data-variable="light-dark(var(--color), var(--bg))" class="ruleview-variable" data-variable-computed="light-dark(chartreuse, seagreen)">' +
            '--theme-color' +
          '</span>' +
        ')' +
      '</span>',
    computed:
      // prettier-ignore
      `light-dark(` +
        `<span xmlns="http://www.w3.org/1999/xhtml" data-color="chartreuse">` +
          `<span ` +
            `class="ruleview-swatch ruleview-colorswatch" ` +
            `style="background-color:chartreuse" ` +
            `tabindex="0" ` +
            `role="button" ` +
            `data-color-function="light-dark">` +
          `</span>` +
          `<span class="ruleview-color">chartreuse</span>` +
        `</span>, ` +
        `<span xmlns="http://www.w3.org/1999/xhtml" data-color="seagreen" class="ruleview-unmatched">` +
          `<span ` +
            `class="ruleview-swatch ruleview-colorswatch" ` +
            `style="background-color:seagreen" ` +
            `tabindex="0" ` +
            `role="button" ` +
            `data-color-function="light-dark">` +
          `</span>` +
          `<span class="ruleview-color">seagreen</span>` +
        `</span>` +
      `)`,
  });

  await assertVariableTooltipForProperty(view, "div", "background", {
    header:
      // prettier-ignore
      '<span xmlns="http://www.w3.org/1999/xhtml">' +
        'var(' +
          '<span data-variable="" class="ruleview-variable">' +
            '--empty' +
          '</span>' +
        ')' +
      '</span>',
    computed: "&lt;empty&gt;",
    computedClasses: ["empty-css-variable"],
  });

  await assertVariableTooltipForProperty(view, "*", "--nested-with-empty", {
    header: "&lt;empty&gt;",
    headerClasses: ["empty-css-variable"],
  });
});
