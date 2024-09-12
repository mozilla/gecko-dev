/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test css properties that are inactive on block-level elements.

const TEST_URI = `
<style>
  #block {
    border: 1px solid #000;
    vertical-align: sub;
  }
  td {
    vertical-align: super;
  }
  #flex {
    display: inline-flex;
    vertical-align: text-bottom;
  }
  main {
    vertical-align: middle;
  }
  main::before {
    content: 'hello';
    vertical-align: middle;
    height: 10px;
  }
</style>
<h1 style="vertical-align:text-bottom;">browser_rules_inactive_css_inline.js</h1>
<div id="block">Block</div>
<table>
  <tr><td>A table cell</td></tr>
</table>
<div id="flex">Inline flex element</div>
<main></main>
`;

const TEST_DATA = [
  {
    selector: "h1",
    inactiveDeclarations: [
      {
        declaration: { "vertical-align": "text-bottom" },
        ruleIndex: 0,
      },
    ],
  },
  {
    selector: "#block",
    inactiveDeclarations: [
      {
        declaration: { "vertical-align": "sub" },
        ruleIndex: 1,
      },
    ],
  },
  {
    selector: "td",
    activeDeclarations: [
      {
        declarations: { "vertical-align": "super" },
        ruleIndex: 1,
      },
    ],
  },
  {
    selector: "#flex",
    activeDeclarations: [
      {
        declarations: { "vertical-align": "text-bottom" },
        ruleIndex: 1,
      },
    ],
  },
  {
    selector: "main",
    activeDeclarations: [
      {
        declarations: { "vertical-align": "middle" },
        // The ::before rule in the pseudo-element section
        ruleIndex: [1, 0],
      },
    ],
    inactiveDeclarations: [
      {
        declaration: { height: "10px" },
        // The ::before rule in the pseudo-element section
        ruleIndex: [1, 0],
      },
      {
        declaration: { "vertical-align": "middle" },
        ruleIndex: 4,
      },
    ],
  },
  {
    // Select the "main::before" node
    selectNode: async inspector => {
      const node = await getNodeFront("main", inspector);
      const children = await inspector.markup.walker.children(node);
      const beforeElement = children.nodes[0];
      await selectNode(beforeElement, inspector);
    },
    activeDeclarations: [
      {
        declarations: { "vertical-align": "middle" },
        ruleIndex: 0,
      },
    ],
    inactiveDeclarations: [
      {
        declaration: { height: "10px" },
        ruleIndex: 0,
      },
    ],
  },
];

add_task(async function () {
  await pushPref("devtools.inspector.inactive.css.enabled", true);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await runInactiveCSSTests(view, inspector, TEST_DATA);
});
