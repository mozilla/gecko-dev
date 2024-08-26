/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct when the page defines layers.

const TEST_URI = `
  <style type="text/css">
    @import url(${URL_ROOT_COM_SSL}doc_imported_anonymous_layer.css) layer;
    @import url(${URL_ROOT_COM_SSL}doc_imported_named_layer.css) layer(importedLayer);
    @import url(${URL_ROOT_COM_SSL}doc_imported_no_layer.css);

    @layer myLayer {
      h1, [test-hint=named-layer] {
        background-color: tomato;
        color: lightgreen;
      }
    }

    @layer {
      h1, [test-hint=anonymous-layer] {
        color: green;
        font-variant: small-caps
      }
    }

    h1, [test-hint=no-rule-layer] {
      color: pink;
    }
  </style>
  <h1>Hello @layer!</h1>
`;

add_task(async function () {
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);

  const expectedRules = [
    { selector: "element", ancestorRulesData: null },
    { selector: `h1, [test-hint="no-rule-layer"]`, ancestorRulesData: null },
    {
      selector: `h1, [test-hint="imported-no-layer--no-rule-layer"]`,
      ancestorRulesData: null,
    },
    {
      selector: `h1, [test-hint="anonymous-layer"]`,
      ancestorRulesData: ["@layer {"],
    },
    {
      selector: `h1, [test-hint="named-layer"]`,
      ancestorRulesData: ["@layer myLayer {"],
    },
    {
      selector: `h1, [test-hint="imported-named-layer--no-rule-layer"]`,
      ancestorRulesData: ["@layer importedLayer {", "  @media screen {"],
    },
    {
      selector: `h1, [test-hint="imported-named-layer--named-layer"]`,
      ancestorRulesData: [
        "@layer importedLayer {",
        "  @media screen {",
        "    @layer in-imported-stylesheet {",
      ],
    },
    {
      selector: `h1, [test-hint="imported-nested-named-layer--named-layer"]`,
      ancestorRulesData: [
        "@layer importedLayer {",
        "  @layer importedNestedLayer {",
        "    @layer in-imported-nested-stylesheet {",
      ],
    },
    {
      selector: `h1, [test-hint="imported-anonymous-layer--no-rule-layer"]`,
      ancestorRulesData: ["@layer {"],
    },
  ];

  const rulesInView = Array.from(view.element.children);
  is(
    rulesInView.length,
    expectedRules.length,
    "All expected rules are displayed"
  );

  for (let i = 0; i < expectedRules.length; i++) {
    const expectedRule = expectedRules[i];
    info(`Checking rule #${i}: ${expectedRule.selector}`);

    const selector = rulesInView[i].querySelector(
      ".ruleview-selectors-container"
    ).innerText;
    is(selector, expectedRule.selector, `Expected selector for ${selector}`);

    if (expectedRule.ancestorRulesData == null) {
      is(
        getRuleViewAncestorRulesDataElementByIndex(view, i),
        null,
        `No ancestor rules data displayed for ${selector}`
      );
    } else {
      is(
        getRuleViewAncestorRulesDataTextByIndex(view, i),
        expectedRule.ancestorRulesData.join("\n"),
        `Expected ancestor rules data displayed for ${selector}`
      );
    }
  }
});

add_task(async function editStylesheetLayerRule() {
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(`
        <link rel="stylesheet" href="${URL_ROOT_COM_SSL}doc_layer_edit.css">
        <h1>Editing @layer stylesheet</h1>
      `)
  );

  const { inspector, view } = await openRuleView();

  info("Select h1 node");
  await selectNode("h1", inspector);

  is(
    await getComputedStyleProperty("h1", null, "font-size"),
    "20px",
    "original font-size value for h1 is 20px"
  );

  const prop = getTextProperty(view, 1, { "font-size": "20px" });

  info("Change font-size");
  await setProperty(view, prop, "42px");

  is(
    await getComputedStyleProperty("h1", null, "font-size"),
    "42px",
    "h1 font-size was properly set"
  );
});
