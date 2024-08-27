/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that custom properties are displayed in the computed view.

const TEST_URI = `
  <style type="text/css">
    @property --registered-color {
      syntax: '<color>';
      inherits: true;
      initial-value: rgb(0, 100, 200);
    }

    @property --registered-color-secondary {
      syntax: '<color>';
      inherits: true;
      initial-value: rgb(200, 100, 00);
    }

    @property --registered-length {
      syntax: '<length>';
      inherits: false;
      initial-value: 10px;
    }

    /* This property should not be used. It shares the same suffix than previous property
       names to assert our check isn't too loose */
    @property --registered {
      syntax: '<length>';
      inherits: false;
      initial-value: 10px;
    }

    body {
      --global-custom-property: red;
      /* invalid at computed value time */
      --registered-color-secondary: 1em;
    }

    main {
      --registered-color-secondary: rgb(3, 7, 11);
    }

    h1 {
      color: var(--global-custom-property);
    }

    #match-1 {
      --global-custom-property: blue;
      --custom-property-1: lime;
      /* invalid at computed value time */
      --registered-color-secondary: 10px;
    }
    #match-2 {
      --global-custom-property: gold;
      --custom-property-2: cyan;
      --custom-property-empty: ;
      --registered-color-secondary: rgb(13, 17, 23);
      border: var(--registered-length) solid var( /* color */ --registered-color );
    }
  </style>
  <main>
    <h1 id="match-1">Hello</h1>
    <h1 id="match-2">World</h1>
  <main>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();

  await assertComputedPropertiesForNode(inspector, view, "body", [
    {
      name: "--global-custom-property",
      value: "red",
      matchedRules: [
        {
          selector: "body",
          value: "red",
          invalidAtComputedValueTime: false,
        },
      ],
    },
    {
      name: "--registered-color-secondary",
      value: "rgb(200, 100, 0)",
      invalidAtComputedValueTime: false,
      syntax: "<color>",
      matchedRules: [
        {
          selector: "body",
          value: "1em",
          invalidAtComputedValueTime: true,
        },
        {
          selector: "initial-value",
          value: "rgb(200, 100, 00)",
          invalidAtComputedValueTime: false,
        },
      ],
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "main", [
    {
      name: "--global-custom-property",
      value: "red",
      matchedRules: [
        {
          selector: "body",
          value: "red",
          invalidAtComputedValueTime: false,
        },
      ],
    },
    {
      name: "--registered-color-secondary",
      value: "rgb(3, 7, 11)",
      syntax: "<color>",
      matchedRules: [
        {
          selector: "main",
          value: "rgb(3, 7, 11)",
          invalidAtComputedValueTime: false,
        },
        {
          selector: "body",
          value: "1em",
          invalidAtComputedValueTime: true,
        },
        {
          selector: "initial-value",
          value: "rgb(200, 100, 00)",
          invalidAtComputedValueTime: false,
        },
      ],
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "#match-1", [
    {
      name: "color",
      value: "rgb(0, 0, 255)",
    },
    {
      name: "--custom-property-1",
      value: "lime",
    },
    {
      name: "--global-custom-property",
      value: "blue",
    },
    {
      name: "--registered-color-secondary",
      // value inherited from `main`, as the one set in #match-1 is invalid at comptued value time
      value: "rgb(3, 7, 11)",
      syntax: "<color>",
      matchedRules: [
        {
          selector: "#match-1",
          value: "10px",
          invalidAtComputedValueTime: true,
        },
        {
          selector: "main",
          value: "rgb(3, 7, 11)",
          invalidAtComputedValueTime: false,
        },
        {
          selector: "body",
          value: "1em",
          invalidAtComputedValueTime: true,
        },
        {
          selector: "initial-value",
          value: "rgb(200, 100, 00)",
          invalidAtComputedValueTime: false,
        },
      ],
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "#match-2", [
    {
      name: "color",
      value: "rgb(255, 215, 0)",
    },
    {
      name: "--custom-property-2",
      value: "cyan",
    },
    {
      name: "--custom-property-empty",
      value: "<empty>",
    },
    {
      name: "--global-custom-property",
      value: "gold",
    },
    {
      name: "--registered-color",
      value: "rgb(0, 100, 200)",
      matchedRules: [
        {
          selector: "initial-value",
          value: "rgb(0, 100, 200)",
          invalidAtComputedValueTime: false,
        },
      ],
    },
    {
      name: "--registered-color-secondary",
      value: "rgb(13, 17, 23)",
      syntax: "<color>",
      matchedRules: [
        {
          selector: "#match-2",
          value: "rgb(13, 17, 23)",
          invalidAtComputedValueTime: false,
        },
        {
          selector: "main",
          value: "rgb(3, 7, 11)",
          invalidAtComputedValueTime: false,
        },
        {
          selector: "body",
          value: "1em",
          invalidAtComputedValueTime: true,
        },
        {
          selector: "initial-value",
          value: "rgb(200, 100, 00)",
          invalidAtComputedValueTime: false,
        },
      ],
    },
    {
      name: "--registered-length",
      value: "10px",
      matchedRules: [
        {
          selector: "initial-value",
          value: "10px",
          invalidAtComputedValueTime: false,
        },
      ],
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "html", []);
});

async function assertComputedPropertiesForNode(
  inspector,
  view,
  selector,
  expected
) {
  const onRefreshed = inspector.once("computed-view-refreshed");
  await selectNode(selector, inspector);
  await onRefreshed;

  const computedItems = getComputedViewProperties(view);
  is(
    computedItems.length,
    expected.length,
    `Computed view has the expected number of items for "${selector}"`
  );
  for (let i = 0; i < computedItems.length; i++) {
    const expectedData = expected[i];
    const computedEl = computedItems[i];
    const nameSpan = computedEl.querySelector(".computed-property-name");
    const propertyName = nameSpan.firstChild.textContent;
    const valueSpan = computedEl.querySelector(".computed-property-value");
    const iacvtIcon = computedEl.querySelector(
      ".computed-property-value-container .invalid-at-computed-value-time-warning:not([hidden])"
    );

    is(
      propertyName,
      expectedData.name,
      `computed item #${i} for "${selector}" is the expected one`
    );
    is(
      valueSpan.textContent,
      expectedData.value,
      `computed item #${i} for "${selector}" has expected value`
    );
    if (expectedData.invalidAtComputedValueTime) {
      ok(
        !!iacvtIcon,
        `computed item #${i} for "${selector}" has the invalid-at-computed-value-time icon`
      );
      is(
        iacvtIcon.getAttribute("title"),
        `Property value does not match expected "${expectedData.syntax}" syntax`,
        `iacvt icon on computed item #${i} for "${selector}" has expected title`
      );
    } else {
      is(
        iacvtIcon,
        null,
        `computed item #${i} for "${selector}" does not have the invalid-at-computed-value-time icon`
      );
    }

    if (expectedData.matchedRules) {
      info(`Check matched rules for computed item #${i} for "${selector}"`);
      const matchedRulesContainerEl = await getComputedViewMatchedRules(
        view,
        propertyName
      );
      const matchedRulesEls = matchedRulesContainerEl.querySelectorAll("p");
      is(
        matchedRulesEls.length,
        expectedData.matchedRules.length,
        `computed item #${i} for "${selector}" have the expected number of matched rules`
      );

      for (let j = 0; j < matchedRulesEls.length; j++) {
        const expectedMatchRuleData = expectedData.matchedRules[j];
        const matchedRuleEl = matchedRulesEls[j];
        const matchedRuleSelector =
          matchedRuleEl.querySelector(".fix-get-selection").textContent;
        const matchedRuleValue = matchedRuleEl.querySelector(
          ".computed-other-property-value"
        ).textContent;
        const matchedRuleIacvtIcon = matchedRuleEl.querySelector(
          ".invalid-at-computed-value-time-warning"
        );
        is(
          matchedRuleSelector,
          expectedMatchRuleData.selector,
          `Got expected selector for matched rule #${j} of computed item #${i} for "${selector}"`
        );
        is(
          matchedRuleValue,
          expectedMatchRuleData.value,
          `Got expected value for matched rule #${j} of computed item #${i} for "${selector}"`
        );

        if (expectedMatchRuleData.invalidAtComputedValueTime) {
          ok(
            !!matchedRuleIacvtIcon,
            `matched rule #${j} of computed item #${i} for "${selector}" has the invalid-at-computed-value-time icon`
          );
          is(
            matchedRuleIacvtIcon.getAttribute("title"),
            `Property value does not match expected "${expectedData.syntax}" syntax`,
            `iacvt icon on computed item #${i} for "${expectedMatchRuleData.selector}" has expected title`
          );
        } else {
          is(
            matchedRuleIacvtIcon,
            null,
            `matched rule #${j} of computed item #${i} for "${selector}" does not have the invalid-at-computed-value-time icon`
          );
        }
      }

      // Close the match rules to avoid issues in next iteration
      const onMatchedRulesCollapsed = inspector.once(
        "computed-view-property-collapsed"
      );
      computedEl.querySelector(".computed-expandable").click();
      await onMatchedRulesCollapsed;
    }
  }
}
