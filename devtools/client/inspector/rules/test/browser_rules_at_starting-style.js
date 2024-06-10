/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view properly handles @starting-style rules.

const TEST_URI = `
  <style>
    h1, [data-test="top-level"] {
      color: tomato;
      transition: all 1s;

      @starting-style {
        color: gold;
      }
    }

    @starting-style {
      body, [data-test="in-starting-style"] {
        color: navy;
      }

      @layer {
        body, [data-test="in-starting-style-layer"] {
          color: hotpink;
        }
      }

      h1, [data-test="in-starting-style"] {
        background-color: salmon;
      }
    }
  </style>
  <h1>Hello @starting-style!</h1>`;

add_task(async function () {
  await pushPref("layout.css.starting-style-at-rules.enabled", true);
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();
  await assertRules("body", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `body, [data-test="in-starting-style"]`,
      ancestorRulesData: ["@starting-style {"],
    },
    {
      selector: `body, [data-test="in-starting-style-layer"]`,
      ancestorRulesData: ["@starting-style {", "  @layer {"],
    },
  ]);

  await assertRules("h1", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `&`,
      ancestorRulesData: [
        `h1, [data-test="top-level"] {`,
        "  @starting-style {",
      ],
    },
    {
      selector: `h1, [data-test="in-starting-style"]`,
      ancestorRulesData: ["@starting-style {"],
    },
    {
      selector: `h1, [data-test="top-level"]`,
      ancestorRulesData: null,
    },
  ]);

  // TODO: Check overridden and "transitioned" properties

  async function assertRules(nodeSelector, expectedRules) {
    await selectNode(nodeSelector, inspector);
    const rulesInView = Array.from(
      view.element.querySelectorAll(".ruleview-rule")
    );
    is(
      rulesInView.length,
      expectedRules.length,
      `[${nodeSelector}] All expected rules are displayed`
    );

    for (let i = 0; i < expectedRules.length; i++) {
      const expectedRule = expectedRules[i];
      info(`[${nodeSelector}] Checking rule #${i}: ${expectedRule.selector}`);

      const selector = rulesInView[i].querySelector(
        ".ruleview-selectors-container"
      )?.innerText;

      is(
        selector,
        expectedRule.selector,
        `[${nodeSelector}] Expected selector for rule #${i}`
      );

      const isInherited = rulesInView[i].matches(
        ".ruleview-header-inherited + .ruleview-rule"
      );
      if (expectedRule.inherited) {
        ok(isInherited, `[${nodeSelector}] rule #${i} is inherited`);
      } else {
        ok(!isInherited, `[${nodeSelector}] rule #${i} is not inherited`);
      }

      if (expectedRule.ancestorRulesData == null) {
        is(
          getRuleViewAncestorRulesDataElementByIndex(view, i),
          null,
          `[${nodeSelector}] No ancestor rules data displayed for ${selector}`
        );
      } else {
        is(
          getRuleViewAncestorRulesDataTextByIndex(view, i),
          expectedRule.ancestorRulesData.join("\n"),
          `[${nodeSelector}] Expected ancestor rules data displayed for ${selector}`
        );
      }
    }
  }
});
